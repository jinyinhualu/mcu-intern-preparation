# AddRmsNorm 学习进度日志

这份文件记录 AddRmsNorm 算子的第一轮逻辑学习过程。当前阶段只关注题面、数学公式、形状变化、数据类型和边界语义，不进入 Ascend C 代码实现。

## 2026-09-11：第一次认识 AddRmsNorm

### 算子的一句话理解

AddRmsNorm 可以拆成三步：

```text
x = x1 + x2
rstd = 1 / sqrt(mean(x^2, last_dim) + epsilon)
y = (x * rstd) * gamma
```

它把 Transformer 中常见的“残差相加 + RMSNorm”融合到一个算子中，并同时输出三个结果：

```text
y    ：归一化并乘 gamma 后的结果
rstd ：每一行的倒数标准差，固定为 float32
x    ：x1 + x2 的残差和
```

### 与 AddCMul 的区别

AddCMul 的计算通常是：

```text
y[i] = input[i] + x1[i] * x2[i] * value
```

每个元素可以独立计算，属于典型的 element-wise 算子。

AddRmsNorm 的计算是：

```text
x = x1 + x2
rstd = 根据整行 x 计算
y = x * rstd * gamma
```

`y[i][j]` 不只依赖当前元素，还依赖当前行最后一维上的全部元素，因为 `rstd[i]` 是一整行归约得到的。这使它同时具有：

```text
逐元素计算 + 最后一维归约 + 按行广播
```

### RMSNorm 与 LayerNorm 的区别

本题中的 RMSNorm 不进行均值中心化，不计算：

```text
x - mean(x)
```

它只计算平方均值：

```text
mean(x^2)
```

因此本题的统计量是：

```text
mean_i = (x[i][0]^2 + x[i][1]^2 + ... + x[i][N-1]^2) / N
rstd_i = 1 / sqrt(mean_i + epsilon)
```

### 逻辑上的二维化

如果输入形状为：

```text
[d0, d1, ..., dk, N]
```

可以把所有前导维展开成行数：

```text
numRow = d0 * d1 * ... * dk
last_dim = N
```

逻辑上转化为：

```text
x1     : [numRow, N]
x2     : [numRow, N]
x       : [numRow, N]
gamma  : [N]
rstd   : [numRow]
y       : [numRow, N]
```

实际输入仍然是原来的 ND Tensor，这种二维化只是为了理解最后一维归约和后续实现。

### 输出形状

对于输入：

```text
x1.shape = [2, 3, 4]
x2.shape = [2, 3, 4]
gamma.shape = [4]
```

有：

```text
numRow = 2 * 3 = 6
N = 4

x.shape = [2, 3, 4]
y.shape = [2, 3, 4]
rstd.shape = [2, 3]
```

`rstd` 的形状始终等于输入形状去掉最后一维：

```text
rstd.shape = x1.shape[:-1]
```

每一个前导位置对应一个 `rstd`，每一个 `rstd` 对应最后一维上的一整行。

### gamma 和 rstd 的广播

两者的广播方向不同。

`gamma.shape = [N]`，沿最后一维变化：

```text
y[row][j] = ... * gamma[j]
```

所有行共享同一个 gamma 向量。

`rstd.shape = [numRow]`，每行一个值，沿最后一维广播：

```text
y[row][j] = x[row][j] * rstd[row] * gamma[j]
```

例如：

```text
x.shape = [2, 3]
gamma = [g0, g1, g2]
rstd = [r0, r1]
```

则：

```text
y[0][0] = x[0][0] * r0 * g0
y[0][1] = x[0][1] * r0 * g1
y[0][2] = x[0][2] * r0 * g2

y[1][0] = x[1][0] * r1 * g0
y[1][1] = x[1][1] * r1 * g1
y[1][2] = x[1][2] * r1 * g2
```

### 手算示例

输入：

```text
x1    = [1, 2]
x2    = [1, -2]
gamma = [2, 0.5]
epsilon = 1
```

步骤一：

```text
x = x1 + x2 = [2, 0]
```

步骤二：

```text
mean(x^2) = (2^2 + 0^2) / 2 = 2
rstd = 1 / sqrt(2 + 1) = 1 / sqrt(3)
```

步骤三：

```text
y[0] = 2 * (1/sqrt(3)) * 2 = 4/sqrt(3)
y[1] = 0 * (1/sqrt(3)) * 0.5 = 0
```

因此：

```text
y    = [4/sqrt(3), 0]
rstd = [1/sqrt(3)]
x    = [2, 0]
```

## 关键特殊情况

### 全零行

如果某一行的 `x` 全为零：

```text
mean(x^2) = 0
rstd = 1 / sqrt(epsilon)
```

例如 `epsilon = 1e-6`：

```text
rstd = 1000.0
```

但是：

```text
y = x * rstd * gamma = 0
```

epsilon 保证了分母不为零，因此不会发生除零。

### epsilon 的位置

正确公式是：

```text
rstd = 1 / sqrt(mean(x^2) + epsilon)
```

epsilon 位于平方根内部，不能误写成：

```text
1 / sqrt(mean(x^2)) + epsilon
```

也不能误写成：

```text
1 / (sqrt(mean(x^2)) + epsilon)
```

epsilon 越大，通常得到的 `rstd` 越小；当 epsilon 很小时，`rstd` 主要由 `mean(x^2)` 决定。

### 内部计算精度和输出精度

题面支持：

```text
float16 / float32 / bfloat16
```

对于 fp16 或 bf16 输入，推荐将中间计算提升到 float32：

```text
x1、x2、gamma -> float32 中间值
x1 + x2       -> float32
x^2           -> float32
求和与除法     -> float32
sqrt 和倒数    -> float32
```

输出规则仍然是：

```text
x    -> 与输入相同 dtype
y    -> 与输入相同 dtype
rstd -> 固定 float32
```

因此不能因为内部使用 float32，就把 `x` 或 `y` 的输出类型也改成 float32。

### 非 32 对齐的最后一维

例如：

```text
x1.shape = [3, 4, 19]
```

逻辑上：

```text
numRow = 3 * 4 = 12
N = 19
```

每行仍然只计算 19 个有效元素，数学公式不变。非 32 对齐主要影响底层：

```text
数据搬运可能需要补齐
向量计算可能存在尾块
尾块不能读取或写出有效范围之外
```

### NaN 和 Inf

题面说明 NaN/Inf 按 IEEE 规则传播，但测试不覆盖。学习阶段先把重点放在正常数值、全零行、epsilon 边界和非对齐尾块上。

## 当前已掌握与待确认

已经形成的理解：

1. AddRmsNorm 是“相加、按最后一维求 RMS、归一化并缩放”的融合算子。
2. `x` 是真实的 `x1 + x2`，不是归一化结果。
3. `rstd` 每行一个值，形状是输入去掉最后一维。
4. RMSNorm 不减均值，只计算 `mean(x^2)`。
5. `gamma` 沿最后一维广播，`rstd` 沿每行的最后一维广播。
6. fp16/bf16 的统计和中间计算应优先使用 float32。
7. 全零行通过 epsilon 得到有限的 rstd，最终 y 仍为零。

尚未确认：

- 能否独立完成单行和多行的完整手算。
- 能否准确写出任意 rank 2~4 输入对应的 `rstd` 形状。
- 能否区分“最后一维归约”和“对所有元素整体归约”。
- 能否解释尾块为什么只处理有效元素。
- 能否把公式映射到 Host tiling、Device kernel 和片上内存搬运。

## 第一轮自测题

### 题 1：单行基础计算

```text
x1    = [1, 2]
x2    = [3, 4]
gamma = [1, 2]
epsilon = 1
```

请计算：

1. `x`
2. `mean(x^2)`
3. `rstd`
4. `y`

保留根号形式即可。

### 题 2：RMSNorm 是否减均值

```text
x1 = [1, 3]
x2 = [1, 1]
```

回答：

1. 相加后的 x 是什么？
2. 是否需要先计算 `x - mean(x)`？
3. 这与 LayerNorm 有什么区别？

### 题 3：输出形状

```text
x1.shape = [2, 3, 4, 5]
x2.shape = [2, 3, 4, 5]
gamma.shape = [5]
```

写出：

1. `N`
2. `numRow`
3. `x.shape`
4. `y.shape`
5. `rstd.shape`

### 题 4：按行归约

```text
x1.shape = [2, 3, 4]
```

回答：

1. `rstd` 有多少个元素？
2. `rstd[0, 0]` 对应哪一行？
3. `rstd[1, 2]` 对应哪一行？
4. 每个 rstd 是否只由对应行计算得到？

### 题 5：gamma 和 rstd 的广播

```text
x.shape = [2, 3]
gamma = [1, 2, 3]
rstd = [r0, r1]
```

写出 `y` 的 6 个元素分别使用哪个 `rstd` 和哪个 `gamma`。

### 题 6：全零行

```text
x1 = [0, 0, 0, 0]
x2 = [0, 0, 0, 0]
gamma = [2, 3, 4, 5]
epsilon = 1e-6
```

计算 `x`、`mean(x^2)`、`rstd` 和 `y`，并说明为什么不除零。

### 题 7：epsilon 影响

已知：

```text
mean(x^2) = 4
```

分别计算以下 epsilon 对应的 rstd：

```text
epsilon = 1e-6
epsilon = 0.5
epsilon = 4
```

### 题 8：dtype

输入为 `float16` 时，回答：

1. `x1`、`x2`、`gamma` 的 dtype 是什么？
2. `rstd` 的 dtype 是什么？
3. `x` 和 `y` 的 dtype 是什么？
4. 为什么平方、求和、开方等中间步骤建议使用 float32？

### 题 9：判断正误

判断并说明理由：

1. 只输出 y，不输出 x，也可以认为算子完整。
2. x 必须等于 x1+x2。
3. rstd 可以输出为输入相同的 fp16。
4. rstd 的形状比 x 少一个最后维度。
5. y 必须使用步骤一得到的 x。

### 题 10：综合计算

```text
x1 = [[1, 2, 3],
      [4, 5, 6]]

x2 = [[1, 0, -1],
      [2, 1, 0]]

gamma = [1, 2, 1]
epsilon = 1
```

请完整计算：

1. `x`
2. 两行各自的 `mean(x^2)`
3. 两行各自的 `rstd`
4. `y`
5. `x`、`y`、`rstd` 的形状

## 当前易错点清单

1. 把 RMSNorm 错当成 LayerNorm，额外减去均值。
2. 把所有行合并后只计算一个全局 rstd。
3. 把 rstd 错误地理解为每个元素一个值。
4. 忘记 x 也是必须写出的输出。
5. 把 gamma 当成一个标量，而不是长度为 N 的向量。
6. 把 epsilon 放在平方根外面。
7. 内部使用 float32 后，错误地把 x/y 输出也变成 float32。
8. 非 32 对齐时，补齐元素也参与 RMS 求和。
9. 尾块处理时读取或写出有效范围之外的数据。

## 题面中的一个待确认点

3.3 节声明支持的数据类型是：

```text
float16 / float32 / bfloat16
```

但第五节精度要求中又出现了 `int32`。这两处描述不一致。当前学习和实现理解暂以输入输出定义为准，即优先支持三种浮点类型；正式实现前需要结合实际算子注册定义、测试脚本或 OJ 约束进一步确认。

## 下一步学习顺序

1. 独立完成题 1~6，确认单行、按行归约和两个广播方向。
2. 完成题 7~10，确认 epsilon、dtype 和综合计算。
3. 用二维数组手算多行输入，并明确每一行独立得到一个 rstd。
4. 再把 `[d0,...,dk,N]` 统一转换成 `[numRow,N]` 的逻辑模型。
5. 在 CPU 上先实现“同 shape、连续内存、float32”的正确性版本。
6. 再考虑尾块、float16/bf16、内部 float32 和结果回写。
7. 最后再进入 Ascend C 的 reduce、DataCopy、tiling、多核切分和三路输出。

当前状态：**AddRmsNorm 处于公式和形状理解阶段，尚未开始代码实现。**
