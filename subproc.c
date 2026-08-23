#define _POSIX_C_SOURCE 202405L

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32

#error "Windows not supported"

#else

#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#endif

#include "epsilon.h"
#include "subproc.h"

#if __STDC_VERSION__ < 202311L
#define noreturn _Noreturn
#endif

#define Cstr_to_Estr(a, b) ((ARRAY_Byte*)epsl_Cstr_to_Estr(a, b))

#define Estr_to_Cstr(a) epsl_Estr_to_Cstr((struct Array*)(a))

static void close_real_fd(int fd) {
    if (fd != -1) close(fd);
}

static ProcError *proc_errorf(const char *format, ...) {
    va_list vargs1;
    va_start(vargs1, format);
    va_list vargs2;
    va_copy(vargs2, vargs1);
    
    size_t msg_len = vsnprintf(NULL, 0, format, vargs1);
    char *buffer = epsl_malloc(msg_len + 1);
    vsprintf(buffer, format, vargs2);

    va_end(vargs1);
    va_end(vargs2);

    ProcError *error = epsl_malloc(sizeof(*error));
    error->ref_counter = 0;
    error->msg = Cstr_to_Estr(1, buffer);
    return error;
}

static ProcessResult *result_error(struct ProcError *error) {
    ProcessResult *result = epsl_calloc(1, sizeof(*result));
    error->ref_counter++;
    result->maybe_error = error;
    return result;
}

void DESTRUCT_Process(Process *proc) {
    close_real_fd((int)proc->output_fd);
}

static void subproc_set_env(ARRAY_ProcEnvVal *env_vals) {
    for (uint64_t i = 0; i < env_vals->length; i++) {
        ProcEnvVal *env_val = env_vals->content[i];
        EPSL_STR_TO_C_STR(env_val->name, name);
        EPSL_STR_TO_C_STR(env_val->val, val);
        setenv(name, val, 1);
        CLEANUP_CONV_C_STR(name);
        CLEANUP_CONV_C_STR(val);
    }
}

struct RedirectFDs {
    int in_pipe;
    int stdin_file;
    int out_pipe;
    int stdout_file;
    int stderr_file;
};

noreturn static void subproc_run(ProcInitInfo *info, struct RedirectFDs redirect_fds) {
    subproc_set_env(info->env_vals);

    EPSL_STR_TO_C_STR(info->program, cmd);

    char **args_buffer = epsl_malloc(
        sizeof(char*) * (info->args->length + 1)
    );

    for (uint64_t i = 0; i < info->args->length; i++) {
        args_buffer[i] = Estr_to_Cstr(info->args->content[i]);
    }

    args_buffer[info->args->length] = NULL;

    int dup_status = 0;

    switch (info->stdin_src->mode) {
    case INMODE_NONE:
        break;
    case INMODE_FROMFILE:
        dup_status |= dup2(redirect_fds.stdin_file, 0) == -1;
        break;
    case INMODE_PIPE:
        dup_status |= dup2(redirect_fds.in_pipe, 0) == -1;
        break;
    default:
        epsl_panicf("invalid stdin redirection mode");
    }

    switch (info->stdout_dest->mode) {
    case OUTMODE_NONE:
    case OUTMODE_TOSTDOUT:
        break;
    case OUTMODE_TOSTDERR:
        dup_status |= dup2(2, 1) == -1;
        break;
    case OUTMODE_PIPE:
        dup_status |= dup2(redirect_fds.out_pipe, 1) == -1;
        break;
    case OUTMODE_TOFILE:
        dup_status |= dup2(redirect_fds.stdout_file, 1) == -1;
        break;
    default:
        epsl_panicf("invalid stdout redirection mode");
    }
    
    switch (info->stderr_dest->mode) {
    case OUTMODE_NONE:
    case OUTMODE_TOSTDERR:
        break;
    case OUTMODE_TOSTDOUT:
        dup_status |= dup2(1, 2) == -1;
        break;
    case OUTMODE_PIPE:
        dup_status |= dup2(redirect_fds.out_pipe, 2) == -1;
        break;
    case OUTMODE_TOFILE:
        dup_status |= dup2(redirect_fds.stderr_file, 2) == -1;
        break;
    default:
        epsl_panicf("invalid stderr redirection mode");
    }

    if (dup_status) {
        epsl_panicf("redirection via dup2 failed: %s", strerror(errno));
    }

    close_real_fd(redirect_fds.out_pipe);
    close_real_fd(redirect_fds.stdout_file);
    close_real_fd(redirect_fds.stderr_file);

    execvp(cmd, args_buffer);

    if (errno == ENOENT) {
        fprintf(stderr, "%s: command not found\n", cmd);
    } else {
        fprintf(stderr, "%s exec failed: %s\n", cmd, strerror(errno));
    }

    exit(1);
}

static ProcError *open_redirect_file_fd(NULLABLE_ARRAY_Byte *E_path, int *fd, int flags) {
    if (E_path == NULL) {
        epsl_panicf("expected path when redirecting output to file");
    }
    EPSL_STR_TO_C_STR(E_path, c_path);
    *fd = open(c_path, flags, 0b110110100);
    ProcError *err = NULL;
    if (*fd == -1) {
        err = proc_errorf(
            "cannot redirect to %s: %s", c_path, strerror(errno)
        );
    }
    CLEANUP_CONV_C_STR(c_path);
    return err;
}

static void make_non_blocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

ProcessResult *SPR_start_proc(ProcInitInfo *info) {
    int out_pipe[2] = {-1, -1};

    if (info->stdout_dest->mode == OUTMODE_PIPE
        || info->stderr_dest->mode == OUTMODE_PIPE) {
        if (pipe(out_pipe)) {
            return result_error(proc_errorf(
                "failed to create pipe: %s", strerror(errno)
            ));
        }
        make_non_blocking(out_pipe[0]);
    }

    int in_pipe[2] = {-1, -1};

    if (info->stdin_src->mode == INMODE_PIPE) {
        if (pipe(in_pipe)) {
            return result_error(proc_errorf(
                "failed to create pipe: %s", strerror(errno)
            ));
        }
        make_non_blocking(in_pipe[1]);
    }

    struct RedirectFDs redirect_fds = {
        .in_pipe = in_pipe[0],
        .stdin_file = -1,
        .out_pipe = out_pipe[1],
        .stdout_file = -1,
        .stderr_file = -1,
    };

    if (info->stdin_src->mode == INMODE_FROMFILE) {
        ProcError *err = open_redirect_file_fd(
            info->stdin_src->file, &redirect_fds.stdin_file,
            O_RDONLY
        );
        if (err != NULL) return result_error(err);
    }
    if (info->stdout_dest->mode == OUTMODE_TOFILE) {
        ProcError *err = open_redirect_file_fd(
            info->stdout_dest->file, &redirect_fds.stdout_file,
            O_CREAT | O_TRUNC | O_WRONLY
        );
        if (err != NULL) return result_error(err);
    }
    if (info->stderr_dest->mode == OUTMODE_TOFILE) {
        ProcError *err = open_redirect_file_fd(
            info->stderr_dest->file, &redirect_fds.stderr_file,
            O_CREAT | O_TRUNC | O_WRONLY
        );
        if (err != NULL) return result_error(err);
    }

    pid_t pid = fork();

    if (pid == 0) {
        close_real_fd(in_pipe[1]);
        close_real_fd(out_pipe[0]);
        subproc_run(info, redirect_fds);
    }

    close_real_fd(in_pipe[0]);
    close_real_fd(out_pipe[1]);
    close_real_fd(redirect_fds.stdout_file);
    close_real_fd(redirect_fds.stderr_file);

    if (pid < 0) {
        close_real_fd(in_pipe[1]);
        close_real_fd(out_pipe[0]);
        return result_error(proc_errorf(
            "Failed to start subprocess: %s", strerror(errno)
        ));
    }

    Process *process = epsl_malloc(sizeof(*process));
    process->ref_counter = 1;
    process->program = info->program;
    process->program->ref_counter++;
    process->output_fd = out_pipe[0];
    if (process->output_fd != -1) {
        process->out_data = epsl_blank_array(1);
        process->out_data->ref_counter++;
    } else {
        process->out_data = NULL;
    }
    process->input_fd = in_pipe[1];
    if (process->input_fd != -1) {
        process->in_data = epsl_blank_array(1);
        process->in_data->ref_counter++;
    } else {
        process->in_data = NULL;
    }
    process->pid = pid;
    process->no_new_input = false;
    process->completed = false;
    process->result_status = -1;

    ProcessResult *result = epsl_calloc(1, sizeof(*result));
    result->maybe_process = process;
    return result;
}

static NULLABLE_ProcError *proc_waitpid(Process *process, int options) {
    while (true) {
        int wstatus;
        pid_t s = waitpid(process->pid, &wstatus, options);
        if (s < 0) {
            return proc_errorf("Failed to get process status: %s", strerror(errno));
        } else if (s == 0) {
            return NULL;
        }
        if (WIFEXITED(wstatus)) {
            process->result_status = WEXITSTATUS(wstatus);
            break;
        } else if (WIFSIGNALED(wstatus)) {
            process->result_status = WTERMSIG(wstatus) + 128;
            break;
        }
    }
    process->completed = true;
    return NULL;
}

#define READ_SIZE 32768

static NULLABLE_ProcError *read_fd_to_Estr(int fd, ARRAY_Byte *str) {
    while (true) {
        char buf[READ_SIZE];
        ssize_t byte_count = read(fd, buf, sizeof(buf));
        if (byte_count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return proc_errorf("pipe read() failed: %s", strerror(errno));
        } else if (byte_count == 0) {
            break;
        }
        epsl_increase_capacity(
            (struct Array *)str, str->length + byte_count, 1
        );
        memcpy(str->content + str->length, buf, byte_count);
        str->length += byte_count;
    }
    return NULL; 
}

struct InputBuffer {
    ARRAY_Byte *data;
    uint64_t offset;
};

static NULLABLE_ProcError *write_fd_from_input(int fd, struct InputBuffer *inbuf) {
    ARRAY_Byte *data = inbuf->data;
    while (data->length > inbuf->offset) {
        uint64_t remaining = data->length - inbuf->offset;
        ssize_t write_count = write(
            fd, data->content + inbuf->offset, remaining
        );
        if (write_count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE) {
                break;
            }
            return proc_errorf("pipe write() failed: %s", strerror(errno));
        }
        inbuf->offset += write_count;
    }
    return NULL;
}

static void collapse_input_buffer(struct InputBuffer *inbuf) {
    ARRAY_Byte *data = inbuf->data;
    uint64_t offset = inbuf->offset;
    data->length -= offset;
    memmove(data->content, data->content + offset, data->length);
    inbuf->offset = 0;
}

struct PollArrays {
    nfds_t cnt;
    struct pollfd *poll_fds;
    ARRAY_Byte **outs;
    struct InputBuffer *ins;
    int64_t **stored_ptrs;
};

#define SHIFT_POLLARRAYS_ARR(arrs, i, name) memmove(\
    arrs->name + i, arrs->name + i + 1,\
    sizeof(*arrs->name) * (arrs->cnt - i - 1));

static void remove_poll_arrs_item(struct PollArrays *arrs, nfds_t i) {
    SHIFT_POLLARRAYS_ARR(arrs, i, poll_fds);
    SHIFT_POLLARRAYS_ARR(arrs, i, outs);
    SHIFT_POLLARRAYS_ARR(arrs, i, ins);
    SHIFT_POLLARRAYS_ARR(arrs, i, stored_ptrs);
    arrs->cnt--;
}

enum CommUntil {
    COMMUNTIL_FD_CLOSE       = 0x0,
    COMMUNTIL_READ_COMPLETE  = 0x1,
    COMMUNTIL_WRITE_COMPLETE = 0x2
};

static NULLABLE_ProcError *proc_communicate(Process *process, enum CommUntil until) {
    NULLABLE_ProcError *err = NULL;

    nfds_t fd_count = (process->input_fd != -1)
        + (process->output_fd != -1);
    
    struct PollArrays arrs = {
        fd_count,
        epsl_malloc(sizeof(struct pollfd) * fd_count),
        epsl_calloc(fd_count, sizeof(ARRAY_Byte*)),
        epsl_calloc(fd_count, sizeof(ARRAY_Byte*)),
        epsl_calloc(fd_count, sizeof(int64_t*)),
    };
    
    nfds_t fd_idx = 0;
    if (process->input_fd != -1) {
        arrs.poll_fds[fd_idx] = (struct pollfd){
            (int)process->input_fd, POLLOUT, 0
        };
        arrs.ins[fd_idx] = (struct InputBuffer){
            process->in_data, 0
        };
        arrs.stored_ptrs[fd_idx] = &process->input_fd;
        fd_idx++;
    }
    if (process->output_fd != -1) {
        arrs.poll_fds[fd_idx] = (struct pollfd){
            (int)process->output_fd, POLLIN, 0
        };
        arrs.outs[fd_idx] = process->out_data;
        arrs.stored_ptrs[fd_idx] = &process->output_fd;
        fd_idx++;
    }
    
    while (arrs.cnt > 0) {
        int timeout = (until & COMMUNTIL_READ_COMPLETE) ? 0 : -1;
        int poll_status = poll(arrs.poll_fds, arrs.cnt, timeout);
        if (poll_status == -1) {
            err = proc_errorf("poll() failed: %s", strerror(errno));
            goto exit;
        }

        bool has_read = false;
        bool ongoing_writes = false;
        for (nfds_t i = arrs.cnt; i > 0;) {
            i--;
            struct pollfd poll_fd = arrs.poll_fds[i];
            if (poll_fd.revents & POLLIN) {
                err = read_fd_to_Estr(poll_fd.fd, arrs.outs[i]);
                if (err != NULL) goto exit;
                has_read = true;
            }
            struct InputBuffer *inbuf = &arrs.ins[i];
            if (poll_fd.revents & POLLOUT) {
                err = write_fd_from_input(poll_fd.fd, inbuf);
                if (err != NULL);
            }
            int64_t *stored_ptr = arrs.stored_ptrs[i];
            bool close_fd = false;
            if (poll_fd.revents & POLLHUP) {
                close_fd = true;
            } else if (inbuf->data != NULL) {
                if (inbuf->data->length > inbuf->offset) {
                    ongoing_writes = true;
                } else if (process->no_new_input) {
                    close_fd = true;
                }
            }
            if (close_fd) {
                close(poll_fd.fd);
                if (stored_ptr != NULL) *stored_ptr = -1;
                remove_poll_arrs_item(&arrs, i);
            }
            arrs.poll_fds[i].revents = 0;
        }

        if ((until & COMMUNTIL_READ_COMPLETE) && !has_read) goto exit;
        if ((until & COMMUNTIL_WRITE_COMPLETE) && !ongoing_writes) goto exit;
    }

                
exit:
    for (nfds_t i = 0; i < arrs.cnt; i++) {
        struct InputBuffer *inbuf = &arrs.ins[i];
        if (inbuf->data != NULL) {
            collapse_input_buffer(inbuf);
        }
    }

    free(arrs.poll_fds);
    free(arrs.outs);
    free(arrs.stored_ptrs);
    return err;
}

NULLABLE_ProcError *SPR_update_proc_output(Process *process) {
    if (process->output_fd == -1) return NULL;
    return proc_communicate(process, COMMUNTIL_READ_COMPLETE);
}

NULLABLE_ProcError *SPR_await_proc_input_sent(Process *process) {
    if (process->input_fd == -1) {
        return proc_errorf(
            "Cannot send input to process not accepting input"
        );
    }
    return proc_communicate(process, COMMUNTIL_WRITE_COMPLETE);
}

NULLABLE_ProcError *SPR_await_proc_completion(Process *process) {
    process->no_new_input = true;
    if (process->output_fd != -1) {
        NULLABLE_ProcError *err = proc_communicate(
            process, COMMUNTIL_FD_CLOSE);
        if (err != NULL) return err;
    }
    return proc_waitpid(process, 0);
}

NULLABLE_ProcError *SPR_poll_proc_status(Process *process) {
    return proc_waitpid(process, WNOHANG);
}

struct SignalNamePair {
    const char *name;
    int signal;
};

#define SIGNAL_NAME_PAIR(name) {#name, name}

const struct SignalNamePair signal_names[] = {
    SIGNAL_NAME_PAIR(SIGHUP),
    SIGNAL_NAME_PAIR(SIGINT),
    SIGNAL_NAME_PAIR(SIGQUIT),
    SIGNAL_NAME_PAIR(SIGILL),
    SIGNAL_NAME_PAIR(SIGTRAP),
    SIGNAL_NAME_PAIR(SIGABRT),
    SIGNAL_NAME_PAIR(SIGFPE),
    SIGNAL_NAME_PAIR(SIGKILL),
    SIGNAL_NAME_PAIR(SIGBUS),
    SIGNAL_NAME_PAIR(SIGSEGV),
    SIGNAL_NAME_PAIR(SIGSYS),
    SIGNAL_NAME_PAIR(SIGPIPE),
    SIGNAL_NAME_PAIR(SIGALRM),
    SIGNAL_NAME_PAIR(SIGTERM),
    SIGNAL_NAME_PAIR(SIGURG),
    SIGNAL_NAME_PAIR(SIGSTOP),
    SIGNAL_NAME_PAIR(SIGTSTP),
    SIGNAL_NAME_PAIR(SIGCONT),
    SIGNAL_NAME_PAIR(SIGCHLD),
    SIGNAL_NAME_PAIR(SIGTTIN),
    SIGNAL_NAME_PAIR(SIGTTOU),
    SIGNAL_NAME_PAIR(SIGIO),
    SIGNAL_NAME_PAIR(SIGXCPU),
    SIGNAL_NAME_PAIR(SIGXFSZ),
    SIGNAL_NAME_PAIR(SIGVTALRM),
    SIGNAL_NAME_PAIR(SIGPROF),
    SIGNAL_NAME_PAIR(SIGWINCH),
    SIGNAL_NAME_PAIR(SIGUSR1),
    SIGNAL_NAME_PAIR(SIGUSR2),
};

static int lookup_signal_number(char *name) {
    for (int i = 0; i < sizeof(signal_names) / sizeof(*signal_names); i++) {
        struct SignalNamePair pair = signal_names[i];
        if (strcmp(pair.name, name) == 0) {
            return pair.signal;
        }
    }
    return -1;
}

NULLABLE_ProcError *SPR_send_proc_signal(Process *process, ARRAY_Byte *signame) {
    EPSL_STR_TO_C_STR(signame, c_signame)
    int signal_number = lookup_signal_number(c_signame);
    CLEANUP_CONV_C_STR(c_signame);
    if (signal_number == -1) {
        return proc_errorf("Cannot find signal by specified name");
    }
    if (kill(process->pid, signal_number)) {
        return proc_errorf("Failed to send process signal: %s", strerror(errno));
    }
    return NULL;
}
