/*
 * STM32F103 USART1 blocking driver
 *
 * Register choices follow RM0008 USART chapter:
 *   PA9  : alternate-function push-pull TX, 50 MHz (CRH field = 0xB)
 *   PA10 : floating input RX                         (CRH field = 0x4)
 *   CR1  : UE + TE + RE, M=0 and PCE=0 => 8 data bits, no parity
 *   CR2  : STOP=00 => one stop bit
 *
 * The code is intentionally small and polling based.  It is suitable for
 * learning and for short control messages; a high-rate stream should move to
 * interrupts or DMA so the CPU is not held in these loops.
 */

#include "stm32f10x.h"
#include "uart1_blocking.h"

static UART1_Status UART1_StatusFromFlags(uint32_t status_register)
{
    UART1_Status status = UART1_STATUS_OK;

    if ((status_register & USART_SR_FE) != 0U) {
        status = (UART1_Status)(status | UART1_STATUS_FRAME_ERROR);
    }
    if ((status_register & USART_SR_NE) != 0U) {
        status = (UART1_Status)(status | UART1_STATUS_NOISE_ERROR);
    }
    if ((status_register & USART_SR_ORE) != 0U) {
        status = (UART1_Status)(status | UART1_STATUS_OVERRUN_ERROR);
    }
    if ((status_register & USART_SR_PE) != 0U) {
        status = (UART1_Status)(status | UART1_STATUS_PARITY_ERROR);
    }

    return status;
}

UART1_Status UART1_Init(uint32_t pclk2_hz, uint32_t baudrate)
{
    uint32_t brr;

    if ((pclk2_hz == 0U) || (baudrate == 0U)) {
        return UART1_STATUS_INVALID_ARGUMENT;
    }

    /* For oversampling by 16, BRR is round(PCLK2 / baud). */
    brr = (pclk2_hz + (baudrate / 2U)) / baudrate;
    if ((brr == 0U) || (brr > 0xFFFFU)) {
        return UART1_STATUS_INVALID_ARGUMENT;
    }

    /* USART1, GPIOA and AFIO are all on APB2. */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN |
                     RCC_APB2ENR_AFIOEN |
                     RCC_APB2ENR_USART1EN;

    /* PA9: 1011b = 50 MHz alternate-function push-pull. */
    GPIOA->CRH &= ~(0x0FU << 4U);
    GPIOA->CRH |=  (0x0BU << 4U);

    /* PA10: 0100b = floating input. */
    GPIOA->CRH &= ~(0x0FU << 8U);
    GPIOA->CRH |=  (0x04U << 8U);

    /* Keep the peripheral disabled while frame parameters are changed. */
    USART1->CR1 = 0U;
    USART1->CR2 = 0U; /* STOP=00: one stop bit. */
    USART1->CR3 = 0U;
    USART1->BRR = brr;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    return UART1_STATUS_OK;
}

UART1_Status UART1_SendByte(uint8_t byte)
{
    while ((USART1->SR & USART_SR_TXE) == 0U) {
        /* Blocking by design: wait for an empty transmit data register. */
    }

    USART1->DR = (uint16_t)byte;
    return UART1_STATUS_OK;
}

UART1_Status UART1_SendBuffer(const uint8_t *data, uint16_t length)
{
    uint16_t index;

    if ((length != 0U) && (data == (const uint8_t *)0)) {
        return UART1_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U; index < length; ++index) {
        (void)UART1_SendByte(data[index]);
    }

    /* TXE only means DR can accept another byte; TC means the final stop bit
       has completed, which matters before disabling or changing the UART. */
    while ((USART1->SR & USART_SR_TC) == 0U) {
        /* Blocking by design. */
    }

    return UART1_STATUS_OK;
}

UART1_Status UART1_ReceiveByte(uint8_t *byte)
{
    uint32_t status_register;

    if (byte == (uint8_t *)0) {
        return UART1_STATUS_INVALID_ARGUMENT;
    }

    while ((USART1->SR & USART_SR_RXNE) == 0U) {
        /* Blocking by design: wait for one received data byte. */
    }

    /* RM0008 requires SR to be read before DR for receive error handling. */
    status_register = USART1->SR;
    *byte = (uint8_t)USART1->DR;

    return UART1_StatusFromFlags(status_register);
}
