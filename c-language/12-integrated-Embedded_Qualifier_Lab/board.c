#include "board.h"

volatile SensorRegisters g_sensor_regs = {0u, 0u, 0u};
volatile uint32_t g_system_ticks = 0u;

/* 这个计数器只服务于 board.c，其他模块不应直接修改它。 */
static uint32_t s_board_init_count = 0u;

static void write_status_bit(uint32_t bit, bool enabled)
{
    g_sensor_regs.STATUS = (g_sensor_regs.STATUS & ~(UINT32_C(1) << bit)) | ((enabled ? 1u : 0u) << bit);
    (void)bit;
    (void)enabled;
}

void board_init(void)
{
    g_sensor_regs.CTRL = 0u;
    g_sensor_regs.STATUS = 0u;
    g_sensor_regs.DATA = 0u;
    g_system_ticks = 0u;

    s_board_init_count++;
}

void board_tick_isr(void)
{
    g_system_ticks++;
}

uint32_t board_read_status(void)
{
    return g_sensor_regs.STATUS;
}

uint32_t board_init_count(void)
{
    return s_board_init_count;
}

void board_set_alarm(bool enabled)
{
    write_status_bit(SENSOR_STATUS_ALARM_BIT, enabled);
}

void board_set_invalid(bool enabled)
{
    write_status_bit(SENSOR_STATUS_INVALID_BIT, enabled);
}

