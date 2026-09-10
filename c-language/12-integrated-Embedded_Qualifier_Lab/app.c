#include "app.h"

#include <stdbool.h>

#include "board.h"

/*
 * 这组阈值模拟编译进固件的只读配置，真实项目中通常放在 Flash
 * 的只读区域，或者由校准/参数模块提供。
 */
const AlarmConfig g_alarm_config = {
    -400,
    850,
    300,
    1000u,
    800u
};

/* 文件作用域 static：计数器只允许 app.c 内部访问。 */
static uint32_t s_sample_count = 0u;
static uint32_t s_alarm_count = 0u;
static uint32_t s_invalid_count = 0u;

static bool sample_is_valid(const SensorSample *sample)
{
    if (sample->temperature_x10 >= g_alarm_config.min_valid_temp_x10 &&
        sample->temperature_x10 <= g_alarm_config.max_valid_temp_x10 &&
        sample->humidity_x10 <= g_alarm_config.max_valid_humidity_x10 &&
        sample->humidity_x10 > 0u)
    {
        return true;
    }
    (void)sample;
    return false;
}

static bool sample_is_alarm(const SensorSample *sample)
{
    if (sample->temperature_x10 > g_alarm_config.alarm_temp_x10 ||
        sample->humidity_x10 > g_alarm_config.alarm_humidity_x10)
    {
        return true;
    }
    
    return false;
}

void app_init(void)
{
    s_sample_count = 0u;
    s_alarm_count = 0u;
    s_invalid_count = 0u;

    board_set_alarm(false);
    board_set_invalid(false);
}

SampleResult app_process_sample(const SensorSample *sample)
{
    if (sample == NULL)
    {
        s_invalid_count ++;
        board_set_invalid(true);
        board_set_alarm(false);

        return SAMPLE_REJECTED;
    }

    if (!sample_is_valid(sample))
    {
        s_invalid_count ++;
        board_set_invalid(true);
        board_set_alarm(false);

        return SAMPLE_REJECTED;
    }

    const bool alarm = sample_is_alarm(sample);

    s_invalid_count = 0u;
    s_sample_count++;
    board_set_invalid(false);

    if (alarm)
    {
        s_alarm_count++;
        board_set_alarm(true);
        return SAMPLE_ALARM;
    }
    else
    {
        board_set_alarm(false);
        return SAMPLE_ACCEPTED;
    }
}

uint32_t app_sample_count(void)
{
    return s_sample_count;
}

uint32_t app_alarm_count(void)
{
    return s_alarm_count;
}

uint32_t app_invalid_count(void)
{
    return s_invalid_count;
}
