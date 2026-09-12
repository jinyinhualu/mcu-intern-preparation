#include "command_dispatch.h"

#include <string.h>

size_t command_split_line(char *line,
                          const char *argv[],
                          size_t argv_capacity)
{
    (void)line;
    (void)argv;
    (void)argv_capacity;

    line = strtok(line, " \t\n");
    return 0u;
}

const CommandEntry *command_find(const CommandEntry table[],
                                 size_t table_count,
                                 const char *name)
{
    (void)table;
    (void)table_count;
    (void)name;

    /* TODO: return the table entry whose name exactly matches name. */
    return NULL;
}

void command_set_log_callback(CommandContext *ctx,
                              CommandLogCallback callback,
                              void *user_data)
{
    (void)ctx;
    (void)callback;
    (void)user_data;

    /* TODO: save callback and user_data in the context. */
}

CommandResult command_dispatch(CommandContext *ctx,
                               const CommandEntry table[],
                               size_t table_count,
                               char *line)
{
    (void)ctx;
    (void)table;
    (void)table_count;
    (void)line;

    /* TODO: parse, find, call the handler, count, and notify through callback. */
    return COMMAND_INVALID_INPUT;
}
