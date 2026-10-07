#include "uart_exercise.h"

/* This file is intended to be copied into an STM32F1 HAL/CMSIS project. */
#include "stm32f10x.h"

static volatile uint8_t rx_buffer[UART_EXERCISE_BUFFER_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

volatile uint32_t uart_exercise_hardware_errors;
volatile uint32_t uart_exercise_software_overflow;

UART_ExerciseStatus UART_Exercise_Init(uint32_t pclk2_hz,
                                       uint32_t baudrate)
{
    if (pclk2_hz == 0U || baudrate == 0U)
    {
        return UART_EXERCISE_BAD_ARGUMENT;
    }

    uart_exercise_hardware_errors = 0U;
    uart_exercise_software_overflow = 0U;
    rx_head = 0U;
    rx_tail = 0U;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;

    GPIOA->CRH &= ~(0xFU << 4);
    GPIOA->CRH |= (0xBU << 4);

    GPIOA->CRH &= ~(0xFU << 8);
    GPIOA->CRH |= (0x4U << 8);

    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;
    USART1->BRR = (pclk2_hz + baudrate / 2U) / baudrate;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    return UART_EXERCISE_OK;
}

UART_ExerciseStatus UART_Exercise_SendByte(uint8_t data)
{
    while (!(USART1->SR & USART_SR_TXE));

    USART1->DR = data;
    return UART_EXERCISE_OK;
}

UART_ExerciseStatus UART_Exercise_SendBuffer(const uint8_t *data,
                                             uint32_t length)
{
    if (data == NULL && length != 0U)
    {
        return UART_EXERCISE_BAD_ARGUMENT;
    }
    else if (length == 0U)
    {
        return UART_EXERCISE_OK;
    }

    for (uint32_t i = 0U; i < length; i++)
    {
        UART_ExerciseStatus status = UART_Exercise_SendByte(data[i]);
        if (status != UART_EXERCISE_OK)
        {
            return status;
        }
    }

    while (!(USART1->SR & USART_SR_TC));

    return UART_EXERCISE_OK;
}

UART_ExerciseStatus UART_Exercise_ReceiveByte(uint8_t *data)
{
    if (data == NULL)
    {
        return UART_EXERCISE_BAD_ARGUMENT;
    }

    while (!(USART1->SR & USART_SR_RXNE));

    uint32_t sr = USART1->SR;
    if (sr & USART_SR_FE)
    {
        uart_exercise_hardware_errors |= UART_EXERCISE_FE;
    }
    if (sr & USART_SR_NE)
    {
        uart_exercise_hardware_errors |= UART_EXERCISE_NE;
    }
    if (sr & USART_SR_ORE)
    {
        uart_exercise_hardware_errors |= UART_EXERCISE_ORE;
    }
    if (sr & USART_SR_PE)
    {
        uart_exercise_hardware_errors |= UART_EXERCISE_PE;
    }
    *data = (uint8_t)(USART1->DR);

    return UART_EXERCISE_OK;
}

void UART_Exercise_EnableRxInterrupt(void)
{
    USART1->CR1 |= USART_CR1_RXNEIE;
    NVIC_EnableIRQ(USART1_IRQn);
}

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;
    if (sr & USART_SR_RXNE)
    {
        if (sr & USART_SR_FE)
        {
            uart_exercise_hardware_errors |= UART_EXERCISE_FE;
        }
        if (sr & USART_SR_NE)
        {
            uart_exercise_hardware_errors |= UART_EXERCISE_NE;
        }
        if (sr & USART_SR_ORE)
        {
            uart_exercise_hardware_errors |= UART_EXERCISE_ORE;
        }
        if (sr & USART_SR_PE)
        {
            uart_exercise_hardware_errors |= UART_EXERCISE_PE;
        }

        next = (rx_head + 1) % UART_EXERCISE_BUFFER_SIZE;
        if (next == rx_tail)
        {
            uart_exercise_software_overflow++;
        }
        else
        {
            rx_buffer[rx_head] = (uint8_t)(USART1->DR);
            rx_head = next;
        }
    }
}

UART_ExerciseStatus UART_Exercise_ReadBufferedByte(uint8_t *data)
{

    if (data == NULL)
    {
        return UART_EXERCISE_BAD_ARGUMENT;
    }

    if (rx_head == rx_tail)
    {
        return UART_EXERCISE_EMPTY;
    }

    *data = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % UART_EXERCISE_BUFFER_SIZE;

    return UART_EXERCISE_OK;
}
