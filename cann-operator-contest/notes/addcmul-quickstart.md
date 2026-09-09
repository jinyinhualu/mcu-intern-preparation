# AddCMul 快速入门路线

这份笔记的目标不是把 CANN 全学完，而是让你能围绕一道 AddCMul 题快速建立上机能力。

## 先建立直觉

AddCMul 公式：

```text
y = input_data + x1 * x2 * value
```

把它拆成三步：

```text
tmp1 = x1 * x2
tmp2 = tmp1 * value
y    = input_data + tmp2
```

每个输出元素互不依赖，所以它属于 element-wise 融合算子。它适合入门，因为主要难点不在数学，而在数据怎么切、怎么搬、怎么处理尾巴。

## 用单片机经验类比

| 单片机开发 | CANN/Ascend C 算子开发 |
| --- | --- |
| Flash / SRAM | Global Memory / UB |
| DMA 搬运 | `DataCopy` |
| 外设寄存器配置 | Host 侧 tiling 参数 |
| 中断或任务分工 | 多 AI Core 分块并行 |
| 裸机循环处理 buffer | Kernel 内按 tile 处理 Tensor |

所以你不是从零开始。你已经熟悉“硬件资源有限、数据要分批处理、边界条件必须小心”的思维，这正好能迁移到算子开发。

## 90 分钟学习路线

### 0-10 分钟：只背公式和输入输出

必须能脱口而出：

```text
y[i] = input_data[i] + x1[i] * x2[i] * value[0]
```

同时记住：

- 四个输入 dtype 一样。
- 输出 dtype 和输入一样。
- `value` 是 Tensor 标量，不是普通 C 参数。
- `input_data`、`x1`、`x2` 支持广播。

### 10-30 分钟：先写普通 C 版

先不要想 UB、AI Core、性能，只写一个最朴素的循环：

```cpp
for (int i = 0; i < totalNum; ++i) {
    y[i] = input_data[i] + x1[i] * x2[i] * value[0];
}
```

如果这个都说不清，后面的 Ascend C 只是把混乱搬到了模板里。

### 30-55 分钟：把普通循环改成分块思维

普通 C 版：

```text
一次处理所有元素
```

Ascend C 版：

```text
每个 core 处理一段
每段再按 tile 处理
每个 tile 先搬进 UB
算完再搬回 GM
```

心里要有这张图：

```text
GM input/x1/x2
    |
    | DataCopy
    v
UB inputLocal/x1Local/x2Local
    |
    | Mul -> Muls -> Add
    v
UB yLocal
    |
    | DataCopy
    v
GM y
```

### 55-75 分钟：专攻尾块

题目反复强调非 32 整倍数，本质就是提醒你：

```text
最后一块长度可能不足 tileLen
```

你要养成一个习惯：每次循环都算真实处理长度。

```cpp
int curLen = min(tileLen, remainLen);
```

然后所有搬运、计算、写回都用 `curLen`，不要偷懒用固定 `tileLen`。

### 75-90 分钟：再理解广播

广播不是复制数据，而是改变访问下标。

例子：

```text
output shape = [2, 2]
x2 shape     = [2]
```

逻辑上看起来像：

```text
x2 -> [[x2[0], x2[1]],
       [x2[0], x2[1]]]
```

但真正实现时不要真的复制，只是在访问时让高位维度复用同一段数据。

## 写模板时优先找这些文件

拿到 CANNJudge 模板后，先找：

1. Host 侧 tiling 文件：通常负责 shape、dtype、tiling 参数。
2. Device 侧 kernel 文件：通常包含 `__aicore__` kernel、`DataCopy`、向量计算。
3. 算子原型或注册文件：通常定义输入输出名字和 dtype 约束。
4. 测试脚本：通常会生成随机 Tensor 并和 golden 结果对比。

不要一开始就逐行啃完整工程。先定位“哪里决定总元素数”“哪里搬数据”“哪里算公式”“哪里写输出”。

## 最小可交付版本

如果时间很紧，按这个顺序保命：

1. 同形状、`float32`、连续内存。
2. 尾块处理。
3. `float16`。
4. `int32`。
5. `int8`。
6. 常见广播。
7. 通用广播。

OJ 通常是先卡正确性，再卡性能。正确性没过时，不要急着调性能。

## 自查问题

写完或看完一份 AddCMul 代码后，问自己：

1. `value[0]` 是什么时候从 GM 读出来的？
2. 当前 core 的起始 offset 怎么算？
3. 最后一个 tile 的真实长度是多少？
4. `x2=[1,N]` 广播时，访问 offset 有没有写错？
5. `float16` 是半精度中间计算，还是转成 float32 计算？
6. `totalNum == 0` 会不会除以 0 或 DataCopy 越界？

能回答这 6 个问题，就已经不是“只看过题面”的状态了。
