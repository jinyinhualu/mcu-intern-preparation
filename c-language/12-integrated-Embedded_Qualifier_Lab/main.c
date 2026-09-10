#include <stdint.h>
#include <stdio.h>

#include "app.h"
#include "board.h"

static int g_total_tests = 0;
static int g_failed_tests = 0;

static void check_u32(const char *name, uint32_t actual, uint32_t expected)
{
    g_total_tests++;
    if (actual == expected)
    {
        printf("[PASS] %s => %u (0x%08X)\n", name, actual, actual);
    }
    else
    {
        g_failed_tests++;
        printf("[FAIL] %s => actual %u (0x%08X), expected %u (0x%08X)\n",
               name, actual, actual, expected, expected);
    }
}

static void check_result(const char *name, SampleResult actual, SampleResult expected)
{
    check_u32(name, (uint32_t)actual, (uint32_t)expected);
}

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

static void run_board_tests(void)
{
    puts("\n== Board / volatile / static tests ==");

    board_init();

    check_u32("CTRL after board_init", g_sensor_regs.CTRL, 0u);
    check_u32("STATUS after board_init", g_sensor_regs.STATUS, 0u);
    check_u32("DATA after board_init", g_sensor_regs.DATA, 0u);
    check_u32("system ticks after board_init", g_system_ticks, 0u);
    check_u32("private board init count", board_init_count(), 1u);

    board_tick_isr();
    board_tick_isr();
    board_tick_isr();
    check_u32("system ticks after three ISR calls", g_system_ticks, 3u);
}

static void run_config_and_typedef_tests(void)
{
    puts("\n== const / extern / typedef tests ==");

    check_int("alarm temperature threshold", g_alarm_config.alarm_temp_x10, 300);
    check_u32("alarm humidity threshold", g_alarm_config.alarm_humidity_x10, 800u);
    check_int("sensor sample temperature field", (int)sizeof(((SensorSample *)0)->temperature_x10), 4);
    check_int("sensor sample humidity field", (int)sizeof(((SensorSample *)0)->humidity_x10), 4);
}

static void run_sample_flow_tests(void)
{
    SensorSample normal = {265, 580u};
    SensorSample threshold = {300, 800u};
    SensorSample high_temperature = {301, 580u};
    SensorSample high_humidity = {265, 801u};
    SensorSample invalid_temperature = {-401, 580u};
    SensorSample invalid_humidity = {265, 1001u};

    puts("\n== Sensor application tests ==");

    app_init();

    check_result("normal sample", app_process_sample(&normal), SAMPLE_ACCEPTED);
    check_u32("valid sample count after normal sample", app_sample_count(), 1u);
    check_u32("alarm count after normal sample", app_alarm_count(), 0u);
    check_u32("invalid count after normal sample", app_invalid_count(), 0u);
    check_u32("status after normal sample", board_read_status(), 0u);

    check_result("exact threshold is not alarm", app_process_sample(&threshold), SAMPLE_ACCEPTED);
    check_u32("valid sample count at threshold", app_sample_count(), 2u);
    check_u32("alarm count at threshold", app_alarm_count(), 0u);

    check_result("high temperature sample", app_process_sample(&high_temperature), SAMPLE_ALARM);
    check_u32("alarm count after high temperature", app_alarm_count(), 1u);
    check_u32("alarm bit after high temperature",
              board_read_status() & (UINT32_C(1) << SENSOR_STATUS_ALARM_BIT),
              UINT32_C(1) << SENSOR_STATUS_ALARM_BIT);

    check_result("high humidity sample", app_process_sample(&high_humidity), SAMPLE_ALARM);
    check_u32("alarm count after high humidity", app_alarm_count(), 2u);

    check_result("invalid temperature sample",
                 app_process_sample(&invalid_temperature),
                 SAMPLE_REJECTED);
    check_u32("invalid count after invalid temperature", app_invalid_count(), 1u);
    check_u32("valid count ignores invalid temperature", app_sample_count(), 4u);

    check_result("invalid humidity sample",
                 app_process_sample(&invalid_humidity),
                 SAMPLE_REJECTED);
    check_u32("invalid count after invalid humidity", app_invalid_count(), 2u);
    check_u32("invalid bit after invalid humidity",
              board_read_status() & (UINT32_C(1) << SENSOR_STATUS_INVALID_BIT),
              UINT32_C(1) << SENSOR_STATUS_INVALID_BIT);
    check_u32("invalid sample clears alarm bit",
              board_read_status() & (UINT32_C(1) << SENSOR_STATUS_ALARM_BIT),
              0u);

    check_result("NULL sample", app_process_sample(NULL), SAMPLE_REJECTED);
    check_u32("invalid count after NULL sample", app_invalid_count(), 3u);
    check_u32("valid count ignores NULL sample", app_sample_count(), 4u);
}

int main(void)
{
    puts("Embedded Qualifier Lab");
    puts("Scenario: cold-chain temperature/humidity monitor");

    run_board_tests();
    run_config_and_typedef_tests();
    run_sample_flow_tests();

    printf("\nSummary: %d/%d passed\n",
           g_total_tests - g_failed_tests,
           g_total_tests);

    if (g_failed_tests == 0)
    {
        puts("ALL TESTS PASSED");
    }
    else
    {
        puts("Keep going: complete the TODOs in board.c and app.c.");
    }

    return g_failed_tests == 0 ? 0 : 1;
}

