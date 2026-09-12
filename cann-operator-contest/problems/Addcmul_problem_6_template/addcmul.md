# addcmul 题目拆解

题目链接：https://cannjudge.cn/public/s1/addcmul

PyTorch 参考算子：https://pytorch.org/docs/2.1/generated/torch.addcmul.html#torch-addcmul

## 当前状态

已补全题面核心信息，并已把 CANNJudge 空模板放在 `problems/Addcmul_problem_6_template/`。本文件用于整理语义、实现路线、tiling 思路、测试点和当前模板落地状态。

## 一句话题意

对广播后的每个元素计算：

```text
y = input_data + x1 * x2 * value
```

其中 `value` 是 shape 为 `[1]` 的标量 Tensor，输出 `y` 的 dtype 与输入 dtype 保持一致。

## 输入输出

| 角色 | 名称 | 形状 | dtype | 说明 |
| --- | --- | --- | --- | --- |
| 输入 | `input_data` | 任意多维 | `float16`、`float32`、`int8`、`int32` | 加法基准项 |
| 输入 | `x1` | 任意多维 | 同上 | 乘法第一个输入 |
| 输入 | `x2` | 任意多维 | 同上 | 乘法第二个输入 |
| 输入 | `value` | `[1]` | 同上 | 缩放系数 |
| 输出 | `y` | 广播后形状 | 同输入 | 计算结果 |

题面要求 `input_data`、`x1`、`x2`、`value` 的 dtype 相同。

## 广播规则

采用 NumPy/PyTorch 标准广播语义：

1. 从最右侧维度开始对齐。
2. 两个维度相等，或者其中一个为 `1`，则可以广播。
3. 缺失的高位维度按 `1` 处理。
4. 输出形状取每一维的最大值。
5. 无法广播的组合应在运行时触发错误，通常由框架或 Host 侧 shape 推导处理。

例子：

```text
input_data shape: [2, 3, 4, 5]
x1         shape: [1, 3, 1, 5]
x2         shape: [      4, 1]

右对齐后：
input_data: [2, 3, 4, 5]
x1:         [1, 3, 1, 5]
x2:         [1, 1, 4, 1]
output:     [2, 3, 4, 5]
```

广播后的访问规律：

```text
如果某个输入在该维度大小为 1：
    这一维永远访问坐标 0
否则：
    使用输出坐标访问该输入
```

## 实现分层

### Host 侧要做什么

Host 侧负责“看懂题目规模并告诉 Device 怎么干活”：

1. 读取 `input_data`、`x1`、`x2` 的 shape 和 dtype。
2. 检查 shape 是否可广播，推导输出 shape。
3. 计算输出元素总数 `totalNum`。
4. 为每个输入计算广播后的 stride 信息。
5. 计算 tiling 参数，例如每个 core 处理多少元素、每个 tile 处理多少元素、尾块长度。
6. 把 tiling 数据写入 tiling buffer，供 Device 侧 kernel 读取。

### Device 侧要做什么

Device 侧负责“按 Host 给的任务搬数据、算数据、写数据”：

1. 根据 `GetBlockIdx()` 确定当前 AI Core 负责的输出区间。
2. 按 tile 从 Global Memory 搬 `input_data`、`x1`、`x2` 到 UB。
3. 读取或广播标量 `value`。
4. 执行乘法、缩放、加法。
5. 把结果从 UB 写回 Global Memory。

## 最小计算流程

不考虑广播时，核心逻辑可以先理解成：

```cpp
for (int i = coreStart; i < coreEnd; i += tileLen) {
    copy input_data[i : i + tileLen] -> inputLocal;
    copy x1[i : i + tileLen]         -> x1Local;
    copy x2[i : i + tileLen]         -> x2Local;

    tmp = x1Local * x2Local;
    tmp = tmp * value;
    out = inputLocal + tmp;

    copy out -> y[i : i + tileLen];
}
```

这就是入门时最该先写通的版本：同形状、连续内存、带尾块。

## 广播访问公式

如果需要在 kernel 内支持通用广播，线性输出下标 `outIndex` 要映射到每个输入自己的 offset。

伪代码：

```cpp
int64_t BroadcastOffset(
    int64_t outIndex,
    int rank,
    int64_t outShape[],
    int64_t inputShapeAligned[],
    int64_t inputStride[]
) {
    int64_t offset = 0;
    int64_t remain = outIndex;

    for (int dim = rank - 1; dim >= 0; --dim) {
        int64_t coord = remain % outShape[dim];
        remain = remain / outShape[dim];

        if (inputShapeAligned[dim] != 1) {
            offset += coord * inputStride[dim];
        }
    }

    return offset;
}
```

性能上不要一上来就只写最复杂版本。更稳的路线是：

1. 先写 `input_data`、`x1`、`x2` 完全同形状的 fast path。
2. 再支持常见广播，例如 `[32, 32]` 与 `[1, 32]`。
3. 最后补通用多维广播 offset。

## tiling 思路

核心目标：把 `totalNum` 个输出元素分给多个 AI Core，每个 core 再分 tile 处理。

常见拆法：

```text
totalNum = 输出元素总数
coreNum = 实际使用 core 数
baseLen = totalNum / coreNum
tail    = totalNum % coreNum

前 tail 个 core 每个处理 baseLen + 1 个元素
后面的 core 每个处理 baseLen 个元素
```

每个 core 内部：

```text
coreStart = 当前 core 的起始输出下标
coreLen   = 当前 core 的输出元素数

每次处理 tileLen 个元素
最后不足 tileLen 的部分按真实长度处理
```

非 32 整倍数维度的本质问题不是公式变了，而是最后一段数据长度可能不是向量块或搬运块的整数倍。入门时要始终记住：尾块按真实元素数计算，不能读写越界。

## dtype 策略

| dtype | 建议实现策略 | 注意点 |
| --- | --- | --- |
| `float32` | 直接用 float32 向量乘加 | 误差要求 `1e-4` |
| `float16` | 优先考虑中间转 float32 计算再转回 float16 | 更容易满足 `1e-3` 精度 |
| `int32` | 用 int32 计算 | 关注乘法中间值是否可能越界 |
| `int8` | 优先用更宽整数中间类型计算，再转回 int8 | 若最终结果超出 int8 表达范围，要以题目/OJ 语义为准 |

题面说要避免中间计算溢出，因此整数类型不要只凭最窄 dtype 做中间乘法。实际能否使用更宽中间类型，要看 CANNJudge 模板允许的 Ascend C API。

## 空张量

测试范围提到零元素张量。处理原则：

```text
totalNum == 0 时，不做 DataCopy，不做计算，不写输出。
```

如果 Host 侧已经由框架处理空输出，kernel 可以直接返回。不要让 `totalNum == 0` 进入除法、切分或尾块逻辑。

## 测试用例清单

先用这些 case 检查语义，再看性能：

| 类型 | shape | 重点 |
| --- | --- | --- |
| 基础 | `[3]` | 题面示例，确认公式顺序 |
| 二维广播 | `input=[2,2]`，`x2=[2]` | 最右维广播 |
| 非对齐 | `[3,4,19]` | 尾块、非 32 整倍数 |
| 高维 | `[2,3,4,5]` | 多维线性化 |
| 高维广播 | `[2,3,4,5]`、`[1,3,1,5]`、`[4,1]` | 通用广播 offset |
| 大尺寸 | `[1024,1024]` | 性能和多 core 切分 |
| 零元素 | 含 0 维输出 | 直接返回 |
| dtype | `float16`、`float32`、`int8`、`int32` | 精度和转换 |

## 最容易错的地方

1. 公式写成 `(input_data + x1) * x2 * value`。
2. 忘记 `value` 是 Tensor 标量，需要从 GM 读取。
3. 只测了同形状，没有测广播。
4. 尾块仍按完整 tile 长度读写，导致越界。
5. Host 侧 tiling 字段顺序和 Device 侧读取顺序不一致。
6. `float16` 全程半精度计算导致误差偏大。
7. `int8` 中间乘法溢出。
8. `totalNum == 0` 时仍然参与除法或 DataCopy。

## 当前模板落地状态

模板目录：

```text
problems/Addcmul_problem_6_template/
```

已补内容：

1. Host 侧 `InferShape`：按广播规则推导输出 shape。
2. Host 侧 `InferDataType`：输出 dtype 跟随 `input_data`。
3. Host 侧 `TilingFunc`：计算 `totalLength`、`blockDim`、广播 shape 和 stride。
4. Kernel 侧 `Init`：绑定 GM 输入输出，并读取 `value[0]`。
5. Kernel 侧 `Process`：处理空张量、core 分块、路径选择。
6. Kernel 侧同形状浮点向量路径：`DataCopyPad -> Mul -> Muls -> Add -> DataCopyPad`。
7. Kernel 侧广播/整数逐元素路径：使用广播 offset 逐元素计算。

当前是学习版实现：优先保证结构完整、语义清楚、非对齐尾块安全。性能优化还可以继续加，尤其是 `int32` 向量路径和常见广播 fast path。

## 推荐过题路线

1. 写通同形状 `float32`。
2. 加上尾块处理，专门测 `[3,4,19]`。
3. 支持 `float16`，必要时使用 float32 中间计算。
4. 支持 `int32`。
5. 支持 `int8`。
6. 加广播 fast path：标量 `value`、`x2=[1,N]`。
7. 加通用广播。
8. 再考虑性能优化。

这道题的第一性原理很简单：它不是矩阵乘法，也不是归约；它是 element-wise 融合算子。真正要练的是 shape、stride、tiling、尾块和 dtype。
