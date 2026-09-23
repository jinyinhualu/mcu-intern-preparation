# 中断、NVIC 优先级与按键消抖

学习对象：`STM32F103C8T6`，按现有工程的 `PB0` 按键、`EXTI0` 中断来推导。

## 1. 先把手册中的几个名字分开

| 名字 | 所在层 | 作用 |
| --- | --- | --- |
| GPIOB/PB0 | GPIO | 把引脚配置成输入并读出电平 |
| AFIO_EXTICR1 | AFIO | 把 `EXTI0` 这条线接到 `PA0/PB0/...` 中的一个端口 |
| EXTI0 | EXTI | 监测 0 号输入线的上升沿、下降沿，并产生挂起请求 |
| `EXTI0_IRQn` | NVIC | 处理器侧的中断通道编号，允许/屏蔽并排序中断 |
| `EXTI0_IRQHandler` | 向量表/启动文件 | 中断真正进入的函数入口 |

“向量表中的位置”和“运行时优先级”不是同一个概念。手册第 131～133 页列出的 `EXTI0` 位置 13、地址 `0x58` 是向量表条目；运行时的抢占优先级由 NVIC 的优先级寄存器配置。

## 2. 本轮手册定位

以下页码是数据手册页面底部的印刷页码：

| 页码 | 读到的内容 | 对 PB0 按键的用途 |
| --- | --- | --- |
| 126～127 | `AFIO_EXTICR1` 的 `EXTI0[3:0]` 选择端口 | 写 `0001`，让 `EXTI0` 连接 `PB0` |
| 130 | NVIC 特性：68 个可屏蔽通道、16 个可编程优先等级、4 位优先级 | 说明优先级数量和低延迟处理能力 |
| 131～133 | 中断/异常向量表 | `EXTI0` 是 IRQ 号 6，对应向量表位置 13 |
| 134～136 | EXTI 的屏蔽、触发、挂起和配置流程 | 组成一次 GPIO 外部中断 |
| 137 | GPIO 到 EXTI 线的映射，配置前要打开 AFIO 时钟 | 解释 PB0 为什么能接到 EXTI0 |
| 138 | `EXTI_IMR` | bit 0 写 1 才允许 EXTI0 中断请求 |
| 139 | `EXTI_RTSR`、`EXTI_FTSR` | 选择上升沿、下降沿，或两者都选 |
| 140 | `EXTI_PR` | 发生边沿后置 1；写 1 清除挂起位 |

手册第 130 页还明确说明：STM32F10xxx 使用 4 位中断优先级，因此有 16 个可编程等级。Cortex-M3/NVIC 的更完整寄存器说明由手册引导到独立的 Cortex-M3 编程手册。

## 3. 从 PB0 的电平到 `main` 中的按键事件

采用“按键接地、内部上拉”的电路：松开为高，按下为低。

```text
松开/按下
   ↓
PB0 的 GPIO 输入采样
   ↓
AFIO_EXTICR1 选择 PB0 → EXTI0
   ↓
EXTI0 的边沿检测器
   ↓
EXTI_PR bit0 = 1（挂起）
   ↓  EXTI_IMR bit0 = 1 且 NVIC 已使能
EXTI0_IRQn 被 NVIC 接收
   ↓
EXTI0_IRQHandler()
   ↓
HAL_GPIO_EXTI_IRQHandler() 清除 EXTI_PR bit0
   ↓
HAL_GPIO_EXTI_Callback(GPIO_PIN_0)
```

纯寄存器配置的对应关系可以先写成下面这样，目的是把 HAL 参数反向映射回手册寄存器：

```c
RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

/* PB0：MODE=00、CNF=10，即输入上拉/下拉；ODR0=1 选择上拉 */
GPIOB->CRL &= ~(0xFU << 0);
GPIOB->CRL |=  (0x8U << 0);
GPIOB->BSRR = GPIO_BSRR_BS0;

/* AFIO_EXTICR1 的 EXTI0[3:0]：0001 = PB0 */
AFIO->EXTICR[0] &= ~(0xFU << 0);
AFIO->EXTICR[0] |=  (0x1U << 0);

/* 这里先选择双边沿，软件只在稳定的按下沿产生应用事件 */
EXTI->IMR  |= EXTI_IMR_MR0;
EXTI->RTSR |= EXTI_RTSR_TR0;
EXTI->FTSR |= EXTI_FTSR_TR0;
EXTI->PR = EXTI_PR_PR0;       // 清掉初始化前残留的挂起请求

NVIC_SetPriorityGrouping(NVIC_PriorityGroup_4);
NVIC_SetPriority(EXTI0_IRQn, 7U << (8U - __NVIC_PRIO_BITS), 0U);
NVIC_EnableIRQ(EXTI0_IRQn);
```

实际工程使用 HAL 时，不需要手工同时写上面所有寄存器；`HAL_GPIO_Init()`、`HAL_NVIC_SetPriority()` 和 `HAL_NVIC_EnableIRQ()` 会完成同样的硬件配置。`mcu-32/2-EXTI/key_exti_debounce.c` 给出了可放进 HAL 工程的完整示例。

## 4. NVIC 优先级怎么读

- 优先级数字越小，抢占优先级越高；`0` 比 `7` 更紧急。
- STM32F1 只有 4 个已实现的优先级位，常用 `NVIC_PRIORITYGROUP_4` 把这 4 位全部用于抢占优先级，取值 `0～15`，无子优先级。
- 若使用其他分组，4 位会拆成“抢占优先级 + 子优先级”。抢占优先级不同才能互相打断；抢占优先级相同的挂起中断不能互相打断，子优先级只决定等待时的先后。
- 不要把向量表里的位置 13 当成“优先级 13”。`EXTI0` 的向量位置是固定编号，`NVIC_SetPriority(EXTI0_IRQn, 7, 0)` 才是在设置运行时优先级。
- 按键通常不需要最高优先级。示例给它优先级 7，把更紧急的通信、采样或故障中断留在更高优先级。

## 5. 为什么消抖放到主循环

机械按键在接近稳定电平时会反复通断。EXTI 只能识别边沿，不能判断这是一次按键还是触点抖动，因此双边沿配置可能在几毫秒内产生多次中断。

中断服务路径只做三件很快的事：记录最近一次边沿时间、置一个 `pending` 标志、清除硬件挂起位（HAL 的 IRQ 封装完成）。20 ms 稳定时间到了以后，由主循环任务读取 PB0，并判断稳定状态是否真的发生变化。这样 LED 切换、菜单处理、通信发送等耗时工作都在普通上下文中执行。

共享变量用 `volatile` 声明。主循环复制标志和时间戳时只短暂关中断，避免在“读标志、读时间戳、清标志”的窗口中被新的边沿打断；关闭时间不包含消抖等待或业务处理。

时间判断使用无符号减法：`(now - edge_tick) >= 20U`。这种写法在 `HAL_GetTick()` 的 32 位计数回绕后仍然成立，只要消抖间隔远小于计数器周期。

## 6. 学习完成边界与实物验证

本轮已经能从手册页码推导 PB0→AFIO→EXTI0→NVIC→IRQ Handler 的完整链路，并实现了 ISR 只置标志、主循环做消抖和业务处理的代码。还没有在开发板上编译、烧录、测量；下一步应观察一次按下时 `EXTI_PR` 是否置位、回调次数是否因抖动增加，以及应用层最终是否只收到一个按下事件。

