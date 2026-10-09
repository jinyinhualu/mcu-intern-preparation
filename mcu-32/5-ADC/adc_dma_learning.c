/*
 * ADC + DMA learning examples, consolidated on 2026-10-09.
 *
 * Target: STM32F103, STM32F1 CMSIS/SPL device header stm32f10x.h.
 * Alternatively define ADC_DMA_USE_CUBE_HEADERS and STM32F103xB to use
 * STM32CubeF1 CMSIS headers; the local aliases below preserve lesson names.
 * Assumptions:
 *   - Board code has configured PCLK2 = 72 MHz.
 *   - Board code provides an accurate DelayUs() function.
 *   - This file exclusively owns ADC1, PA0..PA2 as needed, DMA1 channel 1,
 *     and DMA1_Channel1_IRQHandler. Merge with an existing handler if needed.
 *   - Analog inputs stay within the device's permitted ADC input range.
 *
 * Functional paths:
 *   Slow single read: ADC1_InitPA0() -> ADC1_ReadPA0().
 *   Continuous acquisition: initialize PA0 or scan PA0..PA2, then call
 *     ADC1_StartContinuousDMA(). Hardware fills adc_dma_samples repeatedly.
 *   Periodic acquisition: initialize ADC, select TIM3_TRGO, configure DMA,
 *     then start a separately configured TIM3. Full TIM3 setup is NOT included.
 *
 * This is a source summary of teaching examples, not a board-verified driver.
 * Calibration and polling waits have no timeout. There is no main(), clock
 * tree setup, filtering, stable-block copy, processing queue or runtime log.
 * Event counters below report IRQ observations, not guaranteed valid blocks.
 */

#if defined(ADC_DMA_USE_CUBE_HEADERS)
#include "stm32f1xx.h"
#define DMA_CCR1_EN      DMA_CCR_EN
#define DMA_CCR1_MINC    DMA_CCR_MINC
#define DMA_CCR1_CIRC    DMA_CCR_CIRC
#define DMA_CCR1_PSIZE_0 DMA_CCR_PSIZE_0
#define DMA_CCR1_MSIZE_0 DMA_CCR_MSIZE_0
#define DMA_CCR1_PL_1    DMA_CCR_PL_1
#define DMA_CCR1_HTIE    DMA_CCR_HTIE
#define DMA_CCR1_TCIE    DMA_CCR_TCIE
#define DMA_CCR1_TEIE    DMA_CCR_TEIE
#else
#include "stm32f10x.h"
#endif
#include <stdint.h>

#define ADC_DMA_SAMPLE_COUNT 12U

extern void DelayUs(uint32_t us);

volatile uint16_t adc_dma_samples[ADC_DMA_SAMPLE_COUNT];
volatile uint32_t adc_dma_half_events = 0U;
volatile uint32_t adc_dma_complete_events = 0U;
volatile uint32_t adc_dma_error_events = 0U;

/* Common setup: one or three external channels, 55.5-cycle sample time. */
static void ADC1_InitInputs(uint32_t scan_three_channels)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;

    /* PCLK2 / 6 = 12 MHz; STM32F103x8/xB ADC limit is 14 MHz. */
    RCC->CFGR = (RCC->CFGR & ~(3U << 14U)) | (2U << 14U);

    /* Initialization also stops this example's previous DMA acquisition. */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel1->CCR &= ~DMA_CCR1_EN;
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    DMA1->IFCR = DMA_IFCR_CGIF1;
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);

    ADC1->CR2 = 0U;
    DelayUs(2U); /* Off interval exceeds two ADC clock cycles at 12 MHz. */

    ADC1->SMPR1 = 0U;
    ADC1->SQR2 = 0U;

    if (scan_three_channels != 0U) {
        GPIOA->CRL &= ~0x00000FFFU; /* PA0, PA1, PA2: analog inputs. */
        ADC1->CR1 = ADC_CR1_SCAN;
        ADC1->SMPR2 = (5U << 0U) | (5U << 3U) | (5U << 6U);
        ADC1->SQR1 = 2U << 20U; /* L=2 means three conversions. */
        ADC1->SQR3 = (0U << 0U) | (1U << 5U) | (2U << 10U);
    } else {
        GPIOA->CRL &= ~0x0000000FU; /* PA0: analog input. */
        ADC1->CR1 = 0U;
        ADC1->SMPR2 = 5U << 0U; /* SMP0=101: 55.5 cycles. */
        ADC1->SQR1 = 0U;        /* One conversion. */
        ADC1->SQR3 = 0U;        /* First conversion: channel 0. */
    }

    /* Right alignment, CONT=0, DMA=0; EXTSEL=111 selects SWSTART. */
    ADC1->CR2 = ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG;
    ADC1->CR2 |= ADC_CR2_ADON;
    DelayUs(2U); /* ADC analog power-up stabilization. */

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0U) {
    }

    ADC1->CR2 |= ADC_CR2_CAL;
    while ((ADC1->CR2 & ADC_CR2_CAL) != 0U) {
    }

    ADC1->SR = 0U;
}

void ADC1_InitPA0(void)
{
    ADC1_InitInputs(0U);
}

void ADC1_InitScan012(void)
{
    ADC1_InitInputs(1U);
}

/* Use after ADC1_InitPA0(), with no active DMA or timer trigger. */
uint16_t ADC1_ReadPA0(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while ((ADC1->SR & ADC_SR_EOC) == 0U) {
    }

    /* The register read obtains the result and clears EOC. */
    return (uint16_t)ADC1->DR;
}

/* Example endpoint scaling; vref_mv must represent the actual reference. */
uint32_t ADC_RawToMillivolts(uint16_t raw, uint32_t vref_mv)
{
    return ((uint32_t)raw * vref_mv) / 4095U;
}

/*
 * Prepare DMA before starting ADC or the triggering timer.
 * Requires ADC initialization above and no conversions currently in progress.
 * Does not itself trigger ADC. ADC1 DMA requests map to DMA1 channel 1.
 */
void ADC1_PrepareCircularDMA(void)
{
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    DMA1_Channel1->CCR &= ~DMA_CCR1_EN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);

    adc_dma_half_events = 0U;
    adc_dma_complete_events = 0U;
    adc_dma_error_events = 0U;

    /* Addresses, not the current contents of the register or array. */
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)adc_dma_samples;
    DMA1_Channel1->CNDTR = ADC_DMA_SAMPLE_COUNT;

    DMA1_Channel1->CCR =
        DMA_CCR1_MINC    | /* Each destination advances by two bytes. */
        DMA_CCR1_CIRC    | /* Reload count and internal addresses at end. */
        DMA_CCR1_PSIZE_0 | /* PSIZE=01: read a 16-bit data item. */
        DMA_CCR1_MSIZE_0 | /* MSIZE=01: write a 16-bit data item. */
        DMA_CCR1_PL_1   | /* DMA arbitration priority: high. */
        DMA_CCR1_HTIE   |
        DMA_CCR1_TCIE   |
        DMA_CCR1_TEIE;
    /* DIR=0: peripheral -> memory; PINC=0; MEM2MEM=0. */

    ADC1->CR2 |= ADC_CR2_DMA;
    NVIC_SetPriority(DMA1_Channel1_IRQn, 6U); /* Example CMSIS priority. */
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    DMA1_Channel1->CCR |= DMA_CCR1_EN;
}

/* Use directly after ADC1_InitPA0() or ADC1_InitScan012(). */
void ADC1_StartContinuousDMA(void)
{
    ADC1->CR2 |= ADC_CR2_CONT;
    ADC1_PrepareCircularDMA();
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

/*
 * Optional trigger-selection fragment, NOT a complete timer example.
 * Use after ADC initialization, before starting TIM3 or ADC conversions.
 * Follow with ADC1_PrepareCircularDMA(), then configure/start TIM3 elsewhere.
 * TIM3 must generate TRGO at the chosen interval; each interval must allow
 * the whole ADC sequence to finish. Do not call the continuous-start path.
 */
void ADC1_SelectTIM3TRGO(void)
{
    ADC1->CR2 =
        (ADC1->CR2 & ~(ADC_CR2_EXTSEL | ADC_CR2_CONT)) |
        (4U << 17U) | /* EXTSEL=100: TIM3_TRGO for ADC1 regular group. */
        ADC_CR2_EXTTRIG;
}

/*
 * Demonstrates flag handling only. ISR event counters do not make samples
 * immutable: the main loop still needs a bounded-time copy/processing scheme.
 * If IRQ service is delayed through several buffer cycles, repeated events
 * can merge into one sticky hardware flag and these counters cannot recover
 * the number of lost cycles. Both halves might already have been overwritten.
 */
void DMA1_Channel1_IRQHandler(void)
{
    uint32_t flags = DMA1->ISR;

    /* Independent if statements: HT and TC may both be pending. */
    if ((flags & DMA_ISR_HTIF1) != 0U) {
        DMA1->IFCR = DMA_IFCR_CHTIF1;
        ++adc_dma_half_events;
    }

    if ((flags & DMA_ISR_TCIF1) != 0U) {
        DMA1->IFCR = DMA_IFCR_CTCIF1;
        ++adc_dma_complete_events;
    }

    if ((flags & DMA_ISR_TEIF1) != 0U) {
        DMA1->IFCR = DMA_IFCR_CTEIF1;
        ++adc_dma_error_events;
        /* Stop this example; reinitialize ADC before restarting acquisition. */
        DMA1_Channel1->CCR &= ~DMA_CCR1_EN;
        ADC1->CR2 &= ~(ADC_CR2_ADON | ADC_CR2_DMA);
    }
}
