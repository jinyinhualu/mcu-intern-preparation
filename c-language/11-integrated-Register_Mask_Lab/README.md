# 11-integrated-Register_Mask_Lab

## 本次任务

你要在一个模拟电机控制板场景里，独立完成 3 个寄存器位操作宏：

- `SET_BIT(REG, BIT)`
- `CLEAR_BIT(REG, BIT)`
- `READ_FIELD(REG, MASK, POS)`

核心训练点：

- 位运算：`|`、`&`、`~`、`<<`、`>>`
- 寄存器掩码：bit、field、mask、position
- 宏的括号：参数括号和整体表达式括号
- 宏的副作用：避免让 `next_index()` 这类参数被重复求值

## 你先做什么

打开 `main.c` 顶部的 TODO 区，只改这 3 行宏：

```c
#define SET_BIT(REG, BIT)             /* TODO: 在这里写宏 */
#define CLEAR_BIT(REG, BIT)           /* TODO: 在这里写宏 */
#define READ_FIELD(REG, MASK, POS)    (0u) /* TODO: 替换这个占位实现 */
```

先不要改下面的测试函数。等三行宏写完，再编译运行。

## 编译运行

在本目录执行：

```powershell
gcc -Wall -Wextra -std=c11 main.c -o register_mask_lab.exe
.\register_mask_lab.exe
```

验收目标：

```text
ALL TESTS PASSED
```

## 小提示

- 设置某位：把一个只有目标 bit 为 1 的掩码 OR 到寄存器里。
- 清除某位：把目标 bit 掩码取反后 AND 到寄存器里。
- 读取字段：先用 mask 取出字段，再右移到 bit0。
- `SET_BIT(bank[next_index()], next_bit())` 这种测试会检查你的宏有没有重复求值。
- 宏定义末尾不要写分号，让调用处自己写分号。
