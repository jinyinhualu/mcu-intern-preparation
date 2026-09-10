#include "board.h"

volatile SensorRegisters g_sensor_regs = {0u, 0u, 0u};
volatile uint32_t g_system_ticks = 0u;

/* 这个计数器只服务于 board.c，其他模块不应直接修改它。 */
static uint32_t s_board_init_count = 0u;

static void write_status_bit(uint32_t bit, bool enabled)
{
    /*
     * TODO 1:
     * 根据 enabled 设置或清除 g_sensor_regs.STATUS 的 bit 位。
     * 要保留 STATUS 中除目标位以外的内容。
     */
    (void)bit;
    (void)enabled;
}

void board_init(void)
{
    /*
     * TODO 2:
     * 清零三个寄存器、清零 g_system_ticks，
     * 并让 s_board_init_count 只增加一次。
     */
}

void board_tick_isr(void)
{
    /*
     * TODO 3:
     * 模拟 SysTick ISR，每进入一次让 g_system_ticks 增加一次。
     */
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

