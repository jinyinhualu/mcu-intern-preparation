# CANN 算子赛学习分支

这个目录用于记录 CANN 算子赛方向的学习过程。当前阶段的目标不是一次性学完整个 CANN 体系，而是先为学校测试建立最低可用能力：能看懂题面、理解算子公式、读懂工程模板、修改基础算子，并能说明数据搬运和切分逻辑。

## 当前背景

- 学校倾向推荐参加 CANN 算子赛方向。
- 距离学校测试时间很近，优先级是快速建立上机理解能力。
- 个人已有基础：C 语言、单片机开发、电赛经历、基础算法刷题。
- 当前短板：第一次系统接触 CANN、Ascend C、NPU 内存模型和算子工程结构。

## 学习主线

1. 先跑通一个最小算子样例，例如 Add。
2. 再理解 `addcmul` 这类融合算子题：先乘、再加、最后写回。
3. 掌握 Host 侧 tiling 和 Device 侧 kernel 的职责分工。
4. 练习 element-wise 算子：Add、Sub、Mul、Div、ReLU、Abs。
5. 每道题都记录公式、输入输出、切分策略、易错点和提交结果。

## 最小知识框架

```text
Host 侧：
读取 shape / dtype -> 计算 tiling 参数 -> 启动 kernel

Device 侧：
从 Global Memory 搬数据 -> 在片上内存计算 -> 写回 Global Memory
```

必须先搞懂的关键词：

- Tensor：输入输出数据，一般可理解为一段连续数组加 shape 信息。
- Global Memory：全局内存，数据规模大，但访问慢。
- UB：片上缓存，计算前通常要把数据搬到这里。
- DataCopy：负责在 Global Memory 和 UB 之间搬运数据。
- Tiling：把大数组切成多个小块，让 AI Core 分块处理。
- Kernel：真正运行在 NPU 上的计算函数。

## 周五测试前最低目标

- 能解释一个 element-wise 算子的输入、输出和计算公式。
- 能看懂基础算子工程中哪些是模板，哪些是需要补充的代码。
- 能说明为什么需要 tiling，以及每个 core 大概处理多少数据。
- 能把 Add 改成 Mul、Sub、ReLU 这类同结构算子。
- 能根据 OJ 报错判断是编译问题、精度问题、shape 问题还是越界问题。

## 练习目录

- `problems/addcmul.md`：围绕 CANNJudge `addcmul` 题目的拆解和复盘。
- `notes/mental-model.md`：CANN 算子开发的理解笔记。
- `notes/addcmul-quickstart.md`：围绕 AddCMul 的快速入门路线。
- `notes/addcmul/addcmul-study-log.md`：持续记录 AddCMul 的练习过程、已掌握内容、纠错点和下一步。
- `notes/addcmul/addcmul-empty-template-analysis.md`：针对实际测试空工程的逐文件职责、缺口和实现顺序分析。
- `notes/addcmul/template-walkthrough.md`：围绕 CANNJudge 空模板的文件职责导读。
- `notes/addcmul/compile-guide.md`：记录 AddCMul 模板的编译命令、当前断点和后续排错路线。
- `notes/addcmul/no-cann-practice.md`：没有 CANN/NPU 环境时的本地模拟练习路线。
- `notes/addrmsnorm/addrmsnorm-study-log.md`：AddRmsNorm 的公式、按最后一维归约、输出形状、dtype、易错点和自测题学习日志。
- `tools/addcmul_cpu_basic.cpp`：AddCMul 同 shape、连续内存的 C++ 最小实现。
- `tools/addcmul_cpu_sim.py`：纯 Python AddCMul 广播、tiling、尾块模拟器。

## 记录规则

每做一道题，至少记录：

1. 题目公式。
2. 输入输出 Tensor。
3. 核心实现思路。
4. 这次卡在哪里。
5. 最后是否通过。
