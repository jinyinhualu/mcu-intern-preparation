# STM32F103：时钟树、定时器计数与 PWM

学习日期：2026-10-08。本轮完成基础理论、参考手册与代码对照、手算和参数补写；没有修改外部工程，没有进行编译、烧录或示波器实测。详细问答证据与掌握情况见 [2026-10-08 学习日志](../../study-log/2026-10-08.md)。实物验证留到 MCU 理论特训后统一进行。

## 1. 手册与实际代码

资料：[STM32F10xxx参考手册（中文）](../STM32F10xxx参考手册（中文）.pdf)。下列定位采用页面底部印刷页码，本轮这些页面与 PDF 页面编号一致。

| 页码 | 内容 | 本轮用途 |
| --- | --- | --- |
| 56，图8 | 时钟树，APB 与定时器时钟分配规则 | 沿 SYSCLK 追踪到 TIM2 |
| 57～58 | HSI 与 PLL | 理解 HSI 直通、二分频和 PLL 倍频 |
| 59 | SYSCLK 选择与 CSS | 区分时钟输入和故障切换控制 |
| 255 | 预分频器、向上计数 | 理解 PSC+1 与 ARR+1 |
| 289 | OC1M 字段与 PWM 模式1 | 理解 CNT 与 CCR1 的比较 |

外部参考工程：

```text
D:\2-Projects\1-STM32_Project\Keysking\stm32\study_pro
```

先对照 `Encoder_playing` 的时钟配置，再以 `LCD_playing/radar_monitoring` 为 PWM 主例：

- `Core/Src/main.c`：`SystemClock_Config()` 与应用流程。
- `Core/Inc/stm32f1xx_hal_conf.h`：工程声明 HSE_VALUE=8000000U。
- `Core/Src/tim.c`：`MX_TIM2_Init()`、TIM2 时钟使能与 PA0 复用输出。
- `Core/Src/servo.c`：PWM 启动、按脉宽换算并重写比较值。

HSE_VALUE 是软件中的频率声明，不能代替对实际晶振的确认。以下计算以外部晶振确实为 8 MHz、初始化成功为前提。

## 2. 先看懂分支与选择器

时钟树中的梯形符号是多路选择器。多个输入提供备选来源，输出使用其中被选中的一路；不会把输入频率相加。

```text
                         ┌→ 直接送到 SW 的 HSI 输入
HSI，标称8 MHz → 分支点 ──┤
                         └→ ÷2 → 4 MHz → PLLSRC
```

普通分支点把同一时钟送到不同模块，本身不改变频率。HSI 直接作为系统时钟与 HSI÷2后作为 PLL 输入，是两条可选使用路径。

| 选择器 | 选择内容 | 输出去向 |
| --- | --- | --- |
| PLLSRC | HSI÷2 或 HSE支路 | PLL 输入 |
| SW | HSI、HSE 或 PLLCLK | SYSCLK |

HSE 支路还可通过 PLLXTPRE 选择原频率或二分频。本例使用 HSE 原频率。

```c
/* PLL 使用哪个来源 */
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

/* SYSCLK 使用哪个来源 */
RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
```

开启 PLL 与选中 PLLCLK 是两件事。即使 PLL 已经输出 72 MHz，SW 仍选择 HSI 时，SYSCLK 仍为标称 8 MHz。若使用 HSI÷2再经 PLL×9，PLLCLK 为 36 MHz；若使用本例 HSE 8 MHz不分频再经 PLL×9，PLLCLK 为 72 MHz。

CSS 是 Clock Security System，负责监测 HSE。CSS 连到 SW 的线表示故障切换控制，不是第四路时钟输入。CSS 启用后，如果 HSE 直接供给 SYSCLK，或经 PLL 间接供给 SYSCLK，HSE 故障会使系统自动切换到 HSI。正常配置时，先沿选中的时钟路径阅读，再单独理解 CSS 控制路径。

## 3. 从 SYSCLK 到 TIM2 输入时钟

主例的总线配置为：

```c
RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
```

完整主路径：

```text
HSE 8 MHz → PLL×9 → SW选择PLLCLK → SYSCLK 72 MHz
→ AHB÷1 → HCLK 72 MHz
    ├→ APB1÷2 → PCLK1 36 MHz → 定时器时钟×2 → TIM2 72 MHz
    └→ APB2÷1 → PCLK2 72 MHz → 定时器时钟×1 → TIM1 72 MHz
```

STM32F103 对应 APB 分频系数为 1 时，定时器输入时钟等于该 PCLK；APB 分频系数不为 1 时，定时器输入时钟等于该 PCLK 的两倍。这是定时器的时钟分配规则，不能把 USART2 的输入时钟也当作 2×PCLK1。

只把 APB1 从÷2改为÷4、HCLK仍为72 MHz时：

```text
PCLK1 = 72 MHz / 4 = 18 MHz
TIM2输入时钟 = 2 × 18 MHz = 36 MHz
```

后面的×2不会随 APB 分频系数变为×4。这个变更仅用于本轮理论题，外部工程仍是 APB1÷2。

## 4. 定时器内部：PSC、CNT、ARR、CCR

本轮使用内部时钟、向上计数、边沿对齐、PWM 模式1、高有效极性。以下高电平公式还限定 0≤CCR1≤ARR+1。

```text
定时器输入时钟 fTIM
    ↓ PSC预分频，除以PSC+1
计数节拍 fCNT → CNT递增
    ├→ CNT与CCR1比较 → 控制输出电平
    └→ 从0数到ARR，下一个计数节拍回到0 → 下一轮
```

| 参数 | 控制内容 | HAL 对应 |
| --- | --- | --- |
| PSC | CNT 的计数节拍长度 | htim2.Init.Prescaler |
| ARR | 一轮有多少个计数节拍 | htim2.Init.Period |
| CCR1 | 一轮中多少个节拍输出高电平 | sConfigOC.Pulse，通道1 |

```text
fCNT = fTIM / (PSC + 1)
Δt = 1 / fCNT
T = (ARR + 1) / fCNT
fPWM = fCNT / (ARR + 1)
tHIGH = CCR1 / fCNT
D = CCR1 / (ARR + 1)
```

PSC=199表示实际200分频。从CNT=0数到7199，共有7200个计数值，所以ARR=7199对应7200个节拍。PWM模式1在向上计数时，CNT<CCR1为有效电平；本例有效电平是高电平。CCR1=540时，CNT=0～539恰好540个节拍，因此CCR1不再加1。

## 5. 基准代码与手算

主例代码摘录：

```c
htim2.Instance = TIM2;
htim2.Init.Prescaler = 200 - 1;
htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
htim2.Init.Period = 7200 - 1;

sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
sConfigOC.OCMode = TIM_OCMODE_PWM1;
sConfigOC.Pulse = 540;
sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
```

在TIM2输入时钟72 MHz、比较值仍保持540的条件下：

```text
fCNT = 72000000 / 200 = 360000 Hz
Δt = 1 / 360000 ≈ 2.778 μs
T = 7200 / 360000 = 20 ms
fPWM = 50 Hz
tHIGH = 540 / 360000 = 1.5 ms
D = 540 / 7200 = 7.5%
```

| CNT 范围 | 输出 | 持续时间 |
| --- | --- | --- |
| 0～539 | 高电平 | 1.5 ms |
| 540～7199 | 低电平 | 18.5 ms |
| 从7199回到0 | 下一轮开始 | 一轮20 ms |

主例运行时的舵机代码会重新设置CCR1，因此上述值是固定CCR1条件下的预测，不代表完整雷达程序始终输出该脉宽。

## 6. 改参数时，哪些量变化

每题独立从基准配置出发：`fTIM=72 MHz、PSC=199、ARR=7199、CCR1=540`。

| 单独变化 | 周期 | 频率 | 高电平时间 | 占空比 |
| --- | --- | --- | --- | --- |
| 不变，基准配置 | 20 ms | 50 Hz | 1.5 ms | 7.5% |
| CCR1改成1080 | 20 ms | 50 Hz | 3 ms | 15% |
| PSC改成399 | 40 ms | 25 Hz | 3 ms | 7.5% |
| ARR改成3599 | 10 ms | 100 Hz | 1.5 ms | 15% |
| APB1改为÷4，其他保持基准 | 40 ms | 25 Hz | 3 ms | 7.5% |

只改PSC或定时器输入时钟时，整个波形在时间轴上同比例伸缩，高低电平一起变长或变短，占空比不变。只改ARR时，高电平节拍数不变，但整轮节拍数变化；只改CCR时，周期不变，高电平宽度与占空比变化。

本轮只改PSC题最初误答6 ms、15%，相当于又沿用了上一题CCR1=1080。纠正要点是先列清本题全部有效参数，独立题恢复基准。后续APB1÷4题正确答出180000 Hz、40 ms、3 ms、7.5%。

## 7. PWM 不需要 CPU 在中断中生成

最初将持续输出理解为“在中断中比较CCR和ARR”。实际执行者是定时器硬件，比较对象是CNT与CCR1，ARR决定计数边界。

CPU负责配置、启动、修改参数和停止；定时器负责持续计数、比较和切换输出。中断是在相应事件发生时通知CPU，可用于应用处理，不是生成基础PWM的必要步骤。

```c
/* 外设初始化已经成功，TIM2_CH1 已配置为 PWM、PA0 已配置为复用输出。 */
__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 540U);

if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
{
    Error_Handler();
}

while (1)
{
    /* 即使这里没有业务代码，PWM也持续由硬件输出。 */
}
```

上述为学习片段，不是已编译或烧录的完整工程。CPU处理其他任务10 ms，且不改变时钟、停止定时器或改写相关配置时，原本1.5 ms的高电平不会被拖长到10 ms。

## 8. 从目标波形反推配置

给定定时器输入时钟72 MHz，目标CNT计数频率1 MHz、PWM频率1 kHz、占空比25%：

```text
PSC + 1 = 72000000 / 1000000 = 72 → PSC=71
ARR + 1 = 1000000 / 1000 = 1000 → ARR=999
CCR1 = 1000 × 25% = 250
```

本轮正确补出的参数：

```c
htim2.Init.Prescaler = 72 - 1;
htim2.Init.Period = 1000 - 1;
sConfigOC.Pulse = 250;
```

预测波形为周期1 ms、频率1 kHz、高电平250 μs、低电平750 μs、高电平占空比25%。CCR1无需减1，因为CNT=0～249已有250个节拍。

## 9. 模拟波形异常与代码检查

模拟题：目标周期20 ms、高电平1.5 ms；假设测得40 ms、3 ms，占空比仍为7.5%。已确认有效ARR=7199、CCR1=540时：

```text
fCNT = (ARR + 1) / T
     = 7200 / 0.040
     = 180000 Hz
```

优先确认定时器所处APB、实际SYSCLK/HCLK/PCLK、APB定时器×2规则，再查PSC；不能只修改CCR把脉宽调回1.5 ms而忽略周期仍错误的问题。若周期正确、只有高电平时间异常，则重点看有效CCR、PWM模式与极性。

实际调试时，应读有效寄存器与运行代码，不只看初始化结构体：

```text
htim2.Init.Prescaler → 初始化PSC
htim2.Init.Period → 初始化ARR
sConfigOC.Pulse → 配置通道1的CCR1
__HAL_TIM_SET_COMPARE(..., TIM_CHANNEL_1, value) → 运行时更新CCR1
```

雷达工程的Servo_SetAngle()调用Servo_SetPulseUs()，按当前时钟和PSC换算比较值，再由__HAL_TIM_SET_COMPARE()写入。固定波形验证时应暂停扫角与其他改写比较值的逻辑。

## 10. 实物阶段的待做验证

本节只保留以后使用的测量计划，没有任何实测通过项。

- 原工程TIM2_CH1使用PA0复用推挽输出；初始化后还要启动PWM。
- 采用固定参数与空业务循环，断开舵机，只测PA0与开发板GND。
- 探头与通道衰减倍率一致，使用直流耦合和上升沿触发。
- 对1 kHz/25%的例子，可从200 μs/div、约1 V/div、约1.5 V触发电平开始调节。
- 相邻上升沿的间隔是周期；同一脉冲上升沿到下降沿的间隔是高电平宽度；用tHIGH/T核对占空比。
- 对照预测值：1 ms、1 kHz、250 μs、750 μs、25%；再做单独改PSC、ARR、CCR的对照实验。

## 11. 本轮完成边界

已完成基础时钟链路、计数与PWM公式、正向手算、参数变化、目标参数补写和模拟故障反推。本轮出现过的选择器、CSS、独立题参数沿用和硬件比较/中断混淆，应在后续间隔复习中回测。

当前没有独立创建完整工程、编译、烧录、实际寄存器调试或示波器操作的验证证据。中心对齐、PWM模式2、预装载与更新事件的动态时序、输入捕获、死区及高级定时器保护不列入本轮掌握范围。本人确认当前仍是理论阶段，本小节可以收尾，实物验证以后统一进行。
