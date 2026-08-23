#include <stdlib.h>

#include "epsilon.h"
#include "inputline.h"

#ifdef USE_GNU_READLINE

#include <readline/readline.h>
#include <readline/history.h>

ARRAY_char *IL_inputline(ARRAY_char *prompt) {
    char *c_prompt = epsl_malloc(prompt->length + 1);
    memcpy(c_prompt, prompt->content, prompt->length);
    c_prompt[prompt->length] = '\0';
    
    char *line = readline(c_prompt);
    free(c_prompt);

    if (line && *line) {
        add_history(line);
    }

    if (!line) line = epsl_calloc(1, 1);

    struct ARRAY_char *result = epsl_malloc(sizeof(*result));
    result->ref_counter = 0;
    result->length = strlen(line);
    result->capacity = result->length + 1;
    result->content = (char*)line;

    return result;
}

#else

ARRAY_char *IL_inputline(ARRAY_char *prompt) {
    epsl_print(prompt);
    return (ARRAY_char*)epsl_read_input_line();
}

#endif
