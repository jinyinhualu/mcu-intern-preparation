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
    /*
     * TODO 4:
     * 判断 sample 的温度和湿度是否都在 g_alarm_config 规定的合法范围内。
     * 这个函数的调用者会先处理 NULL，因此这里不必解引用 NULL。
     */
    (void)sample;
    return false;
}

static bool sample_is_alarm(const SensorSample *sample)
{
    /*
     * TODO 5:
     * 温度或湿度严格超过报警阈值时返回 true。
     */
    (void)sample;
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
        /*
         * TODO 6:
         * 记录一次非法样本，INVALID 置 1，ALARM 清 0，
         * 然后返回 SAMPLE_REJECTED。
         */
        return SAMPLE_REJECTED;
    }

    if (!sample_is_valid(sample))
    {
        /*
         * TODO 7:
         * 处理非 NULL 但数据范围非法的样本。
         */
        return SAMPLE_REJECTED;
    }

    const bool alarm = sample_is_alarm(sample);

    /*
     * TODO 8:
     * 有效样本计数加一，清除 INVALID 状态；
     * 根据 alarm 决定是否增加报警计数、设置 ALARM 状态，
     * 并返回 SAMPLE_ACCEPTED 或 SAMPLE_ALARM。
     */
    (void)alarm;
    return SAMPLE_REJECTED;
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
