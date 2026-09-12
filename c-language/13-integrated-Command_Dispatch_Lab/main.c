#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_dispatch.h"

static int g_total_tests;
static int g_failed_tests;

typedef struct
{
    unsigned int calls;
    CommandResult last_result;
    char last_message[64];
} LogRecorder;

static void check_int(const char *name, int actual, int expected)
{
    g_total_tests++;
    if (actual == expected)
    {
        printf("[PASS] %s => %d\n", name, actual);
    }
    else
    {
        g_failed_tests++;
        printf("[FAIL] %s => actual %d, expected %d\n", name, actual, expected);
    }
}

static void check_uint(const char *name,
                       unsigned int actual,
                       unsigned int expected)
{
    g_total_tests++;
    if (actual == expected)
    {
        printf("[PASS] %s => %u\n", name, actual);
    }
    else
    {
        g_failed_tests++;
        printf("[FAIL] %s => actual %u, expected %u\n", name, actual, expected);
    }
}

static void check_string(const char *name,
                         const char *actual,
                         const char *expected)
{
    g_total_tests++;
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0)
    {
        printf("[PASS] %s => %s\n", name, actual);
    }
    else
    {
        g_failed_tests++;
        printf("[FAIL] %s => actual %s, expected %s\n",
               name,
               actual != NULL ? actual : "(null)",
               expected != NULL ? expected : "(null)");
    }
}

static void check_pointer(const char *name,
                          const void *actual,
                          const void *expected)
{
    g_total_tests++;
    if (actual == expected)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        g_failed_tests++;
        printf("[FAIL] %s => pointer mismatch\n", name);
    }
}

static void record_log(CommandResult result,
                       const char *message,
                       void *user_data)
{
    LogRecorder *recorder = (LogRecorder *)user_data;

    recorder->calls++;
    recorder->last_result = result;
    snprintf(recorder->last_message,
             sizeof(recorder->last_message),
             "%s",
             message != NULL ? message : "");
}

static CommandResult handle_led(CommandContext *ctx,
                                int argc,
                                const char *argv[])
{
    if (argc != 2)
    {
        return COMMAND_BAD_ARGS;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        ctx->led_on = 1;
        return COMMAND_OK;
    }

    if (strcmp(argv[1], "off") == 0)
    {
        ctx->led_on = 0;
        return COMMAND_OK;
    }

    return COMMAND_BAD_ARGS;
}

static CommandResult handle_add(CommandContext *ctx,
                                int argc,
                                const char *argv[])
{
    char *end = NULL;
    long value;

    if (argc != 2)
    {
        return COMMAND_BAD_ARGS;
    }

    value = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || value < -100 || value > 100)
    {
        return COMMAND_BAD_ARGS;
    }

    ctx->counter += (int)value;
    return COMMAND_OK;
}

static CommandResult handle_status(CommandContext *ctx,
                                   int argc,
                                   const char *argv[])
{
    (void)ctx;
    (void)argv;

    return argc == 1 ? COMMAND_OK : COMMAND_BAD_ARGS;
}

static const CommandEntry g_command_table[] =
{
    {"led", "led on|off", handle_led},
    {"add", "add signed_value", handle_add},
    {"status", "status", handle_status}
};

static void run_split_tests(void)
{
    const char *argv[COMMAND_MAX_ARGS];
    char line[] = "  led   on  ";
    char empty[] = " \t\n";
    size_t argc;

    puts("\n== Function pointer / parser tests ==");

    argc = command_split_line(line, argv, COMMAND_MAX_ARGS);
    check_uint("split argument count", (unsigned int)argc, 2u);
    check_string("split command", argv[0], "led");
    check_string("split first argument", argv[1], "on");

    argc = command_split_line(empty, argv, COMMAND_MAX_ARGS);
    check_uint("empty line argument count", (unsigned int)argc, 0u);
}

static void run_find_tests(void)
{
    const CommandEntry *entry;

    puts("\n== Command table lookup tests ==");

    entry = command_find(g_command_table,
                         sizeof(g_command_table) / sizeof(g_command_table[0]),
                         "led");
    check_pointer("find existing command", entry, &g_command_table[0]);
    check_pointer("find missing command",
                  command_find(g_command_table,
                               sizeof(g_command_table) / sizeof(g_command_table[0]),
                               "reboot"),
                  NULL);
}

static void run_dispatch_tests(void)
{
    CommandContext ctx = {0};
    LogRecorder recorder = {0};
    char led_on[] = "led on";
    char add_five[] = "add 5";
    char bad_led[] = "led blink";
    char unknown[] = "reboot";
    char empty[] = "   ";

    puts("\n== Dispatch / callback tests ==");

    command_set_log_callback(&ctx, record_log, &recorder);

    check_int("dispatch led on",
              command_dispatch(&ctx,
                               g_command_table,
                               sizeof(g_command_table) / sizeof(g_command_table[0]),
                               led_on),
              COMMAND_OK);
    check_int("led state after function pointer call", ctx.led_on, 1);
    check_uint("dispatch count after led", ctx.dispatch_count, 1u);

    check_int("dispatch add 5",
              command_dispatch(&ctx,
                               g_command_table,
                               sizeof(g_command_table) / sizeof(g_command_table[0]),
                               add_five),
              COMMAND_OK);
    check_int("counter after add", ctx.counter, 5);
    check_uint("callback calls after successful commands", recorder.calls, 2u);

    check_int("bad led argument",
              command_dispatch(&ctx,
                               g_command_table,
                               sizeof(g_command_table) / sizeof(g_command_table[0]),
                               bad_led),
              COMMAND_BAD_ARGS);
    check_uint("dispatch count after bad handler", ctx.dispatch_count, 3u);

    check_int("unknown command",
              command_dispatch(&ctx,
                               g_command_table,
                               sizeof(g_command_table) / sizeof(g_command_table[0]),
                               unknown),
              COMMAND_UNKNOWN);
    check_uint("error count after unknown command", ctx.error_count, 1u);
    check_uint("callback calls after unknown command", recorder.calls, 4u);
    check_string("last callback message", recorder.last_message, "unknown command");

    check_int("empty command",
              command_dispatch(&ctx,
                               g_command_table,
                               sizeof(g_command_table) / sizeof(g_command_table[0]),
                               empty),
              COMMAND_EMPTY);

    command_set_log_callback(&ctx, NULL, NULL);
    check_int("dispatch with callback cleared",
              command_dispatch(&ctx,
                               g_command_table,
                               sizeof(g_command_table) / sizeof(g_command_table[0]),
                               led_on),
              COMMAND_OK);
}

int main(void)
{
    puts("PC Command Dispatch Lab");
    puts("Scenario: cold-chain terminal command console");

    run_split_tests();
    run_find_tests();
    run_dispatch_tests();

    printf("\nSummary: %d/%d passed\n",
           g_total_tests - g_failed_tests,
           g_total_tests);

    if (g_failed_tests == 0)
    {
        puts("ALL TESTS PASSED");
    }
    else
    {
        puts("Keep going: complete the TODOs in command_dispatch.c.");
    }

    return g_failed_tests == 0 ? 0 : 1;
}
