#ifndef EMBEDDED_QUALIFIER_LAB_BOARD_H
#define EMBEDDED_QUALIFIER_LAB_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_STATUS_ALARM_BIT   0u
#define SENSOR_STATUS_INVALID_BIT 1u

/*
 * 真实 MCU 中，这类结构体通常对应一段外设寄存器布局。
 * 这里用一个全局 volatile 对象模拟硬件寄存器块。
 */
typedef struct
{
    uint32_t CTRL;
    uint32_t STATUS;
    uint32_t DATA;
} SensorRegisters;

extern volatile SensorRegisters g_sensor_regs;
extern volatile uint32_t g_system_ticks;

void board_init(void);
void board_tick_isr(void);
uint32_t board_read_status(void);
uint32_t board_init_count(void);
void board_set_alarm(bool enabled);
void board_set_invalid(bool enabled);

#endif

