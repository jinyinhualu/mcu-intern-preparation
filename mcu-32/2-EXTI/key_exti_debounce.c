/*
 * STM32F103C8T6 / HAL 示例
 *
 * 目标：PB0 按键接地，内部上拉；EXTI0 双边沿触发。
 * ISR 只记录时间和置位标志，消抖及按键业务在 KeyDebounce_Task() 中完成。
 *
 * 接入 CubeIDE/CubeMX 工程时，请把 KEY_Pin、KEY_GPIO_Port、LED_Pin 和
 * LED_GPIO_Port 改成工程中的宏；若工程已经生成 GPIO/NVIC 初始化，可只保留
 * 回调、IRQ Handler 和 KeyDebounce_Task()。
 * 现有 Encoder_playing 工程的宏适配为：
 *   #define KEY_Pin       Key_Enocder_Pin
 *   #define KEY_GPIO_Port Key_Enocder_GPIO_Port
 *   #define LED_Pin       LED_Test_Pin
 *   #define LED_GPIO_Port LED_Test_GPIO_Port
 */

#include "main.h"

#ifndef KEY_Pin
#define KEY_Pin        GPIO_PIN_0
#define KEY_GPIO_Port  GPIOB
#endif

#ifndef LED_Pin
#define LED_Pin        GPIO_PIN_5
#define LED_GPIO_Port  GPIOA
#endif

#define KEY_DEBOUNCE_MS 20U

static volatile uint8_t  key_debounce_pending;
static volatile uint32_t key_last_edge_tick;
static GPIO_PinState     key_stable_state = GPIO_PIN_SET; /* 松开 = 高 */

static void Key_OnPressed(void)
{
    /* 这是应用层工作，故意放在主循环任务中，不放进 ISR。 */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}

/* PB0/EXTI0 的硬件初始化。若由 CubeMX 生成，可用生成代码替代。 */
void Key_EXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    GPIO_InitStruct.Pin = KEY_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

    /* 4 个优先级位全部作为抢占优先级，0 最高、15 最低。 */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 7U, 0U);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    key_stable_state = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
    key_debounce_pending = 0U;
}

/* 启动文件的 EXTI0 向量会调用这个函数。它应保持很短。 */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY_Pin);
}

/* HAL 在清除 EXTI_PR bit0 后调用此回调。这里禁止 HAL_Delay、printf、串口发送等耗时操作。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == KEY_Pin) {
        key_last_edge_tick = HAL_GetTick();
        key_debounce_pending = 1U;
    }
}

/* 在 while (1) 中高频调用，例如：while (1) { KeyDebounce_Task(); App_Task(); } */
void KeyDebounce_Task(void)
{
    uint8_t pending;
    uint32_t edge_tick;
    uint32_t now;

    /* 只保护共享变量的复制；不在关中断期间等待 20 ms。 */
    __disable_irq();
    pending = key_debounce_pending;
    edge_tick = key_last_edge_tick;
    __enable_irq();

    if (pending == 0U) {
        return;
    }

    now = HAL_GetTick();
    if ((uint32_t)(now - edge_tick) < KEY_DEBOUNCE_MS) {
        return;
    }

    /* 如果等待期间又发生边沿，保留新的 pending，让下一轮重新计时。 */
    __disable_irq();
    if (edge_tick != key_last_edge_tick) {
        __enable_irq();
        return;
    }
    key_debounce_pending = 0U;
    __enable_irq();

    /* 读取已经稳定的电平，再做状态变化判断。 */
    GPIO_PinState new_state = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
    if (new_state == key_stable_state) {
        return;
    }

    key_stable_state = new_state;
    if (new_state == GPIO_PIN_RESET) {
        Key_OnPressed();
    }
}
