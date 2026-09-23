/*
 * STM32F103 USART1 blocking driver
 *
 * Assumptions:
 *   - USART1 is connected to PA9 (TX) and PA10 (RX).
 *   - The caller supplies the actual APB2 clock used by USART1.
 *   - The project provides the STM32F1 CMSIS device header.
 *
 * This file intentionally contains no printf/logger implementation.  The
 * status values are kept explicit so a later logger can attach a level,
 * module name and error code without changing the byte transport API.
 */

#ifndef MCU32_UART1_BLOCKING_H
#define MCU32_UART1_BLOCKING_H

#include <stdint.h>

typedef enum {
    UART1_STATUS_OK = 0U,
    UART1_STATUS_INVALID_ARGUMENT = (1U << 0),
    UART1_STATUS_FRAME_ERROR      = (1U << 1),
    UART1_STATUS_NOISE_ERROR      = (1U << 2),
    UART1_STATUS_OVERRUN_ERROR    = (1U << 3),
    UART1_STATUS_PARITY_ERROR     = (1U << 4)
} UART1_Status;

/* Enable clocks, configure PA9/PA10, select 8N1 and set the baud rate. */
UART1_Status UART1_Init(uint32_t pclk2_hz, uint32_t baudrate);

/* Block until the transmit data register is empty, then send one byte. */
UART1_Status UART1_SendByte(uint8_t byte);

/* Send all bytes and block until the final stop bit has left the pin. */
UART1_Status UART1_SendBuffer(const uint8_t *data, uint16_t length);

/*
 * Block until RXNE is set.  On return, *byte contains the received data byte.
 * Reading SR before DR is deliberate: it reports and clears the receive
 * status flags according to the STM32F1 USART register access sequence.
 */
UART1_Status UART1_ReceiveByte(uint8_t *byte);

#endif /* MCU32_UART1_BLOCKING_H */
