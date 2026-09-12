#ifndef COMMAND_DISPATCH_H
#define COMMAND_DISPATCH_H

#include <stddef.h>

#define COMMAND_MAX_ARGS 8u

typedef enum
{
    COMMAND_OK = 0,
    COMMAND_EMPTY = 1,
    COMMAND_UNKNOWN = 2,
    COMMAND_BAD_ARGS = 3,
    COMMAND_INVALID_INPUT = 4
} CommandResult;

typedef struct CommandContext CommandContext;

typedef void (*CommandLogCallback)(CommandResult result,
                                   const char *message,
                                   void *user_data);

typedef CommandResult (*CommandHandler)(CommandContext *ctx,
                                        int argc,
                                        const char *argv[]);

typedef struct
{
    const char *name;
    const char *help;
    CommandHandler handler;
} CommandEntry;

struct CommandContext
{
    int led_on;
    int counter;
    unsigned int dispatch_count;
    unsigned int error_count;
    CommandLogCallback log_callback;
    void *log_user_data;
};

size_t command_split_line(char *line,
                          const char *argv[],
                          size_t argv_capacity);

const CommandEntry *command_find(const CommandEntry table[],
                                 size_t table_count,
                                 const char *name);

void command_set_log_callback(CommandContext *ctx,
                              CommandLogCallback callback,
                              void *user_data);

CommandResult command_dispatch(CommandContext *ctx,
                               const CommandEntry table[],
                               size_t table_count,
                               char *line);

#endif
