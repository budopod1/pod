#define _POSIX_C_SOURCE 202405L

#include <stdlib.h>

#include "epsilon.h"
#include "inputline.h"

#ifdef _WIN32

#error "Windows is not supported"

#else

#include <signal.h>

#endif

#ifdef USE_GNU_READLINE

#include <readline/readline.h>
#include <readline/history.h>

static sig_atomic_t readline_exit_needed = 0;

static void readline_int_handler(int a) {
    readline_exit_needed = 1;
}

static int event_hook_handler(void) {
    if (readline_exit_needed) {
        rl_replace_line("", 0);
        rl_done = 1;
    }
}

extern bool is_sigint_disregarded;

extern void restore_sigint_handler(void);

ARRAY_char *IL_inputline(ARRAY_char *prompt) {
    EPSL_STR_TO_C_STR(prompt, c_prompt);

    if (is_sigint_disregarded) {
        readline_exit_needed = 0;
        struct sigaction act = {0};
        act.sa_handler = &readline_int_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_RESTART;
        sigaction(SIGINT, &act, NULL);
        rl_event_hook = &event_hook_handler;
    }

    char *line = readline(c_prompt);

    CLEANUP_CONV_C_STR(c_prompt);

    if (!line) line = epsl_calloc(1, 1);

    if (*line) {
        HIST_ENTRY *last_entry = history_get(
            history_base + history_length - 1);
        if (last_entry == NULL || strcmp(last_entry->line, line) != 0) {
            add_history(line);   
        }
    }

    struct ARRAY_char *result = epsl_malloc(sizeof(*result));
    result->ref_counter = 0;
    result->length = strlen(line);
    result->capacity = result->length + 1;
    result->content = (char*)line;

    if (is_sigint_disregarded) { 
        restore_sigint_handler();
    }

    return result;
}

#else

ARRAY_char *IL_inputline(ARRAY_char *prompt) {
    epsl_print(prompt);
    return (ARRAY_char*)epsl_read_input_line();
}

#endif
