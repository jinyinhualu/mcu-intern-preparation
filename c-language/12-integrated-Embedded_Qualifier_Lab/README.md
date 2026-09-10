# 12-integrated-Embedded_Qualifier_Lab

## 综合题：冷链箱环境监测终端

你正在给一块低功耗冷链箱监测板补齐应用层代码。板子由三个部分组成：

1. `board.c`：模拟 MCU 底层寄存器和 SysTick 中断。
2. `app.c`：处理温湿度采样、计数和报警。
3. `main.c`：固定测试程序，只负责验收，不要先改它。

本题故意把 `static`、`const`、`volatile`、`extern`、`typedef` 放进一个跨文件工程里。目标不是背出关键字解释，而是能说明：

```text
哪个对象由谁定义？
哪个对象允许谁访问？
哪个值可能被硬件或中断异步改变？
哪个配置应该只读？
哪个类型需要成为模块之间的接口？
```

## 五个关键字的真实嵌入式场景

| 关键字 | 真实场景 | 本工程中的落点 |
| --- | --- | --- |
| `static` | 驱动模块私有状态，例如采样次数、去抖状态、DMA 当前索引；文件外不能直接访问 | `board.c` 的 `s_board_init_count`，`app.c` 的计数器和私有辅助函数 |
| `const` | 校准参数、报警阈值、查表数据、设备信息；通常放到 Flash 的只读区域 | `app.c` 中定义的 `g_alarm_config` |
| `volatile` | 内存映射寄存器、ISR 修改的标志、DMA 更新的缓冲区状态；每次访问都要从内存重新取值 | `g_sensor_regs` 和 `g_system_ticks` |
| `extern` | 一个模块定义对象，其他 `.c` 文件只通过头文件声明来使用 | `board.h` 对寄存器/时基的声明，`app.h` 对报警配置的声明 |
| `typedef` | 把结构体、枚举或固定宽度整数类型变成清晰的驱动接口类型 | `SensorRegisters`、`SensorSample`、`AlarmConfig`、`SampleResult` |

### 重要边界

- 文件作用域的 `static` 是“内部链接”：同名符号可以出现在不同 `.c` 文件里，外部不能通过 `extern` 访问它。
- 函数内部的 `static` 是“静态存储期”：函数返回后值保留，但名字仍只在函数内部可见。
- `const` 表示代码不应通过这个名字修改对象，不等于“绝对不能被任何方式修改”，也不单独保证一定进 Flash。
- `volatile` 不等于原子操作，也不等于线程安全；它只告诉编译器不要把访问优化成一次读取或一次写入。
- `extern` 通常是声明，不分配对象存储；真正的定义应该只出现一次。
- `typedef` 只是类型别名，不会创建一个新的独立类型，也不会自动改变内存布局。

## 你的任务

### 任务 1：补齐 `board.c`

1. 完成 `board_init()`：
   - 清零 `g_sensor_regs.CTRL`、`g_sensor_regs.STATUS`、`g_sensor_regs.DATA`。
   - 清零 `g_system_ticks`。
   - 让 `s_board_init_count` 增加一次。
2. 完成 `board_tick_isr()`：
   - 每进入一次中断模拟函数，`g_system_ticks` 增加一次。
3. 完成 `write_status_bit()`：
   - `enabled == true` 时设置目标位。
   - `enabled == false` 时清除目标位。
   - 不要破坏 `STATUS` 中的其他位。

### 任务 2：补齐 `app.c` 的判断函数

1. 完成 `sample_is_valid()`：
   - `sample == NULL` 不应在这个函数里解引用。
   - 温度合法范围：`-40.0°C` 到 `85.0°C`，代码单位是 `0.1°C`。
   - 湿度合法范围：`0.0%RH` 到 `100.0%RH`，代码单位是 `0.1%RH`。
2. 完成 `sample_is_alarm()`：
   - 温度严格大于 `g_alarm_config.alarm_temp_x10` 时报警。
   - 湿度严格大于 `g_alarm_config.alarm_humidity_x10` 时报警。
   - 任一条件满足即可报警。

### 任务 3：补齐 `app_process_sample()`

要求按下面的行为实现：

| 输入 | 返回值 | 计数变化 | 状态位 |
| --- | --- | --- | --- |
| `NULL` | `SAMPLE_REJECTED` | `invalid + 1` | `INVALID` 置 1，`ALARM` 清 0 |
| 非法温湿度 | `SAMPLE_REJECTED` | `invalid + 1` | `INVALID` 置 1，`ALARM` 清 0 |
| 合法且不报警 | `SAMPLE_ACCEPTED` | `sample + 1` | `INVALID` 清 0，`ALARM` 清 0 |
| 合法且报警 | `SAMPLE_ALARM` | `sample + 1`、`alarm + 1` | `INVALID` 清 0，`ALARM` 置 1 |

注意：非法样本不能增加有效采样计数；达到阈值本身不报警，只有“严格大于”才报警。

### 任务 4：逐段解释

写完后，你需要能逐段解释：

1. 为什么 `g_alarm_config` 的定义不能再写一个 `extern` 变量定义。
2. 为什么 `g_sensor_regs` 和 `g_system_ticks` 需要 `volatile`。
3. 为什么计数器用文件作用域 `static`，而不是直接暴露成全局变量。
4. 为什么接口参数写成 `const SensorSample *sample`。
5. `typedef struct` 对可读性有什么帮助，它是否改变了结构体大小。

## AI 使用规矩

开始写代码前，先自己分析至少 20 分钟。对每一组失败测试，先写下至少两个可能原因，例如：

```text
可能原因 A：状态位清除时误用了赋值，覆盖了 STATUS 的其他位。
可能原因 B：非法样本路径仍然增加了有效采样计数。
我的验证方法：增加一组带有其他状态位的输入，比较修改前后的 STATUS。
```

使用 AI 后：

- 代码必须逐段解释，解释不了的部分暂不合入。
- 每次修改至少测试一条正常路径和一条异常路径。
- 故障复盘写“我如何验证”，不要只写“AI 告诉我答案”。

## 编译运行

在本目录执行：

```powershell
gcc -Wall -Wextra -std=c11 main.c board.c app.c -o embedded_qualifier_lab.exe
.\embedded_qualifier_lab.exe
```

最终目标：

```text
ALL TESTS PASSED
```

建议完成顺序：

1. 只改 `board.c` 的三个 TODO，先让底层测试通过。
2. 再改 `app.c` 的两个判断函数。
3. 最后完成 `app_process_sample()`，一次改一个行为分支。
4. 每次修改后都保留一条正常路径和一条异常路径的测试记录。

