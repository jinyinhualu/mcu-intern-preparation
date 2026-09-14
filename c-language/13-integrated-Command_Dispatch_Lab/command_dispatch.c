#include "command_dispatch.h"

#include <string.h>

size_t command_split_line(char *line, const char *argv[], size_t argv_capacity)
{
    size_t argc = 0;
    char *token = strtok(line, " \t\r\n");

    while (token != NULL && argc < argv_capacity)
    {
        argv[argc] = token;
        argc++;

        token = strtok(NULL, " \t\r\n");
    }

    if (argc > argv_capacity || token != NULL)
    {
        return 0;
    }

    return argc;
}

const CommandEntry *command_find(const CommandEntry table[], size_t table_count, const char *name)
{
    size_t i = 0;

    for (i = 0; i < table_count; i++)
    {
        if (strcmp(table[i].name, name) == 0)
        {
            return &table[i];
        }
    }

    return NULL;
}

void command_set_log_callback(CommandContext *ctx, CommandLogCallback callback, void *user_data)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->log_callback = callback;
    ctx->log_user_data = user_data;
}

CommandResult command_dispatch(CommandContext *ctx, const CommandEntry table[], size_t table_count, char *line)
{
    const char *argv[COMMAND_MAX_ARGS];
    char *line_copy = line;

    size_t argc = command_split_line(line, argv, COMMAND_MAX_ARGS);
    if (argc == 0) return COMMAND_EMPTY;
    if (ctx == NULL || table == NULL || table_count == 0 || line == NULL) return COMMAND_INVALID_INPUT;

    const CommandEntry *entry = command_find(table, table_count, argv[0]);
    if (entry == NULL || entry->handler == NULL)
    {
        ctx->log_callback(COMMAND_UNKNOWN, line, ctx->log_user_data);

        return COMMAND_UNKNOWN;
    }

    else
    {
        CommandResult result = entry->handler(ctx, (int)argc, argv);
        ctx->dispatch_count++;
        if (result != COMMAND_OK)
        {
            ctx->error_count++;
        }
        if (ctx->log_callback != NULL)
        {
            ctx->log_callback(result, line_copy, ctx->log_user_data);
        }
        return result;
    }

    return COMMAND_INVALID_INPUT;
}
