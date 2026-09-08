# 内存分区表

场景：仓库温湿度采集终端  
源码：`main.c`

| 变量 / 表达式 | 类型 | 所属区域 | 判断依据 |
|---|---|---|---|
| `g_device_id` | 已初始化全局变量 | 全局/静态存储区 `.data` | 定义在函数外，且有初值 `1001` |
| `g_error_count` | 未初始化全局变量 | 全局/静态存储区 `.bss` | 定义在函数外，未显式初始化，程序启动时自动清零 |
| `s_total_samples` | 未初始化文件静态变量 | 全局/静态存储区 `.bss` | 函数外定义，带 `static`，未显式初始化 |
| `s_last_alarm` | 未初始化文件静态变量 | 全局/静态存储区 `.bss` | 函数外定义，带 `static`，未显式初始化 |
| `local_static_var` | 已初始化局部静态变量 | 全局/静态存储区 `.data` | 写在函数内部，但带 `static`，生命周期贯穿整个程序 |
| `local_var` | 普通局部变量 | 栈区 | 函数内部普通变量，函数调用时创建，函数返回时销毁 |
| `local_array` | 局部数组 | 栈区 | 函数内部普通数组，属于自动变量 |
| `heap1` | 指针变量本身 | 栈区 | `heap1` 定义在 `show_memory_areas()` 函数内部 |
| `heap1` 指向的内存 | `malloc` 分配的内存 | 堆区 | `malloc` 返回的地址来自堆 |
| `heap2` | 指针变量本身 | 栈区 | `heap2` 定义在 `show_memory_areas()` 函数内部 |
| `heap2` 指向的内存 | `malloc` 分配的内存 | 堆区 | `malloc` 返回的地址来自堆 |
| `frame` | 结构体局部变量 | 栈区 | 定义在 `Temperature_Humidity_Data_Input()` 函数内部 |
| `frame.raw` | 结构体内数组成员 | 栈区 | `frame` 在栈上，因此成员也在同一块栈帧内 |
| `temp` | 普通局部变量 | 栈区 | 定义在函数内部，属于自动变量 |
| `heap_temp` | 指针变量本身 | 栈区 | 定义在 `Temperature_Humidity_Data_Input()` 函数内部 |
| `heap_temp` 指向的内存 | `malloc` 分配的内存 | 堆区 | `malloc` 返回的地址来自堆 |
| `heap_humidity` | 指针变量本身 | 栈区 | 定义在 `Temperature_Humidity_Data_Input()` 函数内部 |
| `heap_humidity` 指向的内存 | `malloc` 分配的内存 | 堆区 | `malloc` 返回的地址来自堆 |
| `frame_buffer` | 局部数组 | 栈区 | 定义在 `demo_out_of_bounds()` 函数内部 |
| `ptr` | 未初始化局部指针变量 | 栈区 | `ptr` 变量本身在栈上，但它保存的地址不确定 |

## 速记

```text
有初始化的全局/static变量 -> .data
未初始化的全局/static变量 -> .bss
普通局部变量/局部数组 -> 栈
malloc申请出来的内存 -> 堆
指针变量本身在哪里，看它定义在哪里
指针指向哪里，看它保存的地址来自哪里
```
