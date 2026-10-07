#ifndef UART_EXERCISE_H
#define UART_EXERCISE_H

#include <stdint.h>

#define UART_EXERCISE_BUFFER_SIZE 64U

typedef enum {
    UART_EXERCISE_OK = 0,
    UART_EXERCISE_EMPTY,
    UART_EXERCISE_FULL,
    UART_EXERCISE_BAD_ARGUMENT,
    UART_EXERCISE_BAD_CLOCK
} UART_ExerciseStatus;

/* Level 1: USART1 polling driver. */
UART_ExerciseStatus UART_Exercise_Init(uint32_t pclk2_hz,
                                       uint32_t baudrate);
UART_ExerciseStatus UART_Exercise_SendByte(uint8_t data);
UART_ExerciseStatus UART_Exercise_SendBuffer(const uint8_t *data,
                                             uint32_t length);
UART_ExerciseStatus UART_Exercise_ReceiveByte(uint8_t *data);

/* Level 2: interrupt RX and a single-producer/single-consumer ring buffer. */
void UART_Exercise_EnableRxInterrupt(void);
void USART1_IRQHandler(void);
UART_ExerciseStatus UART_Exercise_ReadBufferedByte(uint8_t *data);

extern volatile uint32_t uart_exercise_hardware_errors;
extern volatile uint32_t uart_exercise_software_overflow;

#endif
