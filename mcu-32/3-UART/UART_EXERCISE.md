# UART 练习工程

这是一个按关卡完成的 STM32F1 USART1 练习。目标芯片假定为 `STM32F103`，USART1 使用 `PA9/PA10`，时钟假定为 `PCLK2`。代码故意保留 `TODO`，不要一次性把所有关卡都填完。

## 使用方式

把 `uart_exercise.h` 和 `uart_exercise.c` 加入已有的 CMSIS/Cube 工程。当前文件只依赖 `stm32f10x.h`，不依赖 HAL。每完成一关，把修改后的函数贴给我，我会按寄存器位、时序和边界条件检查。

## 关卡

### Level 1：波特率和 GPIO

完成 TODO 1～6：

```text
输入：PCLK2=72 MHz，baudrate=115200
期望：BRR=0x0271，帧格式 8N1，PA9 TX，PA10 RX
```

验收点：

- `USART1` 使用 `PCLK2`；
- 16 倍过采样时，BRR 可按 `round(PCLK2 / baudrate)` 编码；
- PA9 是复用推挽输出，PA10 是浮空输入；
- `UE/TE/RE` 都打开；
- `M=0、PCE=0、STOP=00` 对应 8N1。

### Level 2：轮询收发

完成 TODO 7～9：

- 发送普通字节等待 `TXE`；
- 发送缓冲区最后等待 `TC`；
- 接收先保存 `SR`，再读取 `DR`；
- `FE/NE/ORE/PE` 只能根据保存的 `SR` 判断；
- `0x00` 必须能作为正常数据返回，不能用返回值 0 表示“空”。

建议测试：发送字符串 `ready\r\n`，回环接收 `0x00、0x53、0xFF` 三个值。

### Level 3：RXNE 中断

完成 TODO 10～12：

```text
RXNE 置位
→ RXNEIE 允许 USART 请求
→ NVIC 允许 USART1_IRQn
→ USART1_IRQHandler
→ 先读 SR，再读 DR
→ 记录错误并写入环形缓冲区
```

环形缓冲区采用“留一格”规则：

```text
空：head == tail
满：(head + 1) % SIZE == tail
写：先 rx_buffer[head]，后 head = next
```

缓冲区满时丢弃新字节，保留旧的未读字节，并递增 `uart_exercise_software_overflow`。即使软件缓冲区满，也必须先读取 `DR`，否则可能进一步产生硬件 `ORE`。

### Level 4：主循环取数

完成 TODO 13：

- `data == NULL` 返回 `BAD_ARGUMENT`；
- `head == tail` 返回 `EMPTY`；
- 先读取 `rx_buffer[tail]`，再推进 `tail`；
- 真实收到的 `0x00` 必须与 `EMPTY` 区分。

## 提交给老师的格式

每次只提交一个关卡：

```text
完成关卡：Level X
修改函数：...
代码：...
自测结果：...
不确定的地方：...
```

我会重点检查：`TXE/TC` 是否混用、`SR→DR` 顺序、满/空条件、先写数据后更新 `head`、先读数据后更新 `tail`、错误标志是否被误读，以及是否在中断里做了耗时工作。

## 当前第一步

先只完成 TODO 1～6，不要写发送、接收和中断。完成后把 `UART_Exercise_Init()` 发来，我会逐行批改。
