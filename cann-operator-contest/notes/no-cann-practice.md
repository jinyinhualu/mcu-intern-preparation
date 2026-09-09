# 没有 CANN 环境时怎么练

没有 CANN/NPU 环境时，不要停在“等条件”。AddCMul 这类 element-wise 算子，至少 70% 的理解可以先在普通电脑上练出来。

## 现在能练什么

| 能练的内容 | 对应 CANN 能力 |
| --- | --- |
| 普通循环公式 | Kernel 里的核心计算 |
| 广播 shape 推导 | Host 侧 `InferShape` |
| stride 和 offset | 广播输入的 GM 地址计算 |
| core 分块 | `GetBlockIdx()` 后的任务划分 |
| tile 和尾块 | `DataCopy`/`DataCopyPad` 的边界处理 |
| dtype 误差 | float16/float32/int 的结果检查 |

不能练的是 CANN API 的真实编译、UB 资源限制、NPU 性能和 OJ 运行时行为。这部分要等学校机房、远程服务器或比赛平台。

## 本地模拟器

已新增：

```text
tools/addcmul_cpu_sim.py
```

它用纯 Python 模拟这些动作：

1. Host 侧广播 shape 推导。
2. Host 侧 stride 计算。
3. Host 侧 blockDim 选择。
4. Device 侧 core 分块。
5. Device 侧 tile 循环和尾块。
6. Device 侧 `input + x1 * x2 * value` 计算。

运行测试：

```powershell
python tools\addcmul_cpu_sim.py
```

查看一次广播和 core 切分示例：

```powershell
python tools\addcmul_cpu_sim.py --demo
```

## 练习顺序

### 第 1 轮：只看公式

目标：能独立写出普通 C/Python 循环。

```text
y[i] = input_data[i] + x1[i] * x2[i] * value[0]
```

### 第 2 轮：看 core 分块

在 `tools/addcmul_cpu_sim.py` 里找：

```text
split_core_range
```

确认你能回答：

```text
totalLength=10, blockDim=3 时，每个 core 分别处理哪几段？
```

### 第 3 轮：看广播 offset

在 `tools/addcmul_cpu_sim.py` 里找：

```text
calc_broadcast_offset
```

用这个例子手算一次：

```text
out_shape = [2, 2]
x2_shape  = [1, 2]
```

确认 `out[0]`、`out[1]`、`out[2]`、`out[3]` 分别读 `x2` 的哪个位置。

### 第 4 轮：看尾块

把 `tile_len` 改小，例如改成 `5` 或 `7`，观察测试仍然通过。

你要记住的不是某个固定数字，而是：

```text
curLen = min(tileLen, remain)
```

### 第 5 轮：对照 CANN 模板

把 Python 模拟器和 CANN kernel 对照起来：

| Python 模拟器 | CANN 模板 |
| --- | --- |
| `make_tiling` | `op_host/addcmul.cpp::TilingFunc` |
| `broadcast_shape` | `op_host/addcmul.cpp::CalcBroadcastShape` |
| `split_core_range` | `op_kernel/addcmul.cpp::SplitCoreRange` |
| `calc_broadcast_offset` | `op_kernel/addcmul.cpp::CalcBroadcastOffset` |
| `addcmul_kernel_sim` | `op_kernel/addcmul.cpp::Process` |

## 今天的最低完成标准

1. 能跑通 `python tools\addcmul_cpu_sim.py`。
2. 能看懂 `--demo` 输出里的 `out[i]` 分别读哪个输入下标。
3. 能解释为什么 `[3,4,19]` 会考尾块。
4. 能说清 Host 侧和 Kernel 侧分别在做什么。

做到这些，等你碰到真正 CANN 环境时，就不是从零开始看报错了。
