# AddCMul 编译记录

模板目录：

```text
C:\Users\HP\Documents\ChatGPT\单片机开发实习准备\cann-operator-contest\problems\Addcmul_problem_6_template\code
```

## 本机编译检查结果

当前 Windows 本机可以找到：

```text
cmake
gcc
g++
ninja
```

当前 Windows 本机没有找到：

```text
ASCEND* 环境变量
C:\Ascend
D:\Ascend
E:\Ascend
atc
npu-smi
msopgen
WSL Linux 发行版
```

所以这台机器目前适合编辑代码和学习模板结构，不适合直接完成 CANN 算子编译。

## 已执行过的命令和结果

### 1. 直接配置

```powershell
cmake -S . -B build
```

结果：

```text
No CMAKE_C_COMPILER could be found.
No CMAKE_CXX_COMPILER could be found.
```

原因：CMake 默认选了 Visual Studio 生成器，但本机没有可用的 MSVC C/C++ 编译器环境。

### 2. 改用 Ninja 和 MinGW

```powershell
cmake -S . -B build-ninja2 -G Ninja -DCMAKE_C_COMPILER=E:/MinGW/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=E:/MinGW/mingw64/bin/g++.exe
```

结果：

```text
Could not find a package configuration file provided by "ASC"
ASCConfig.cmake
asc-config.cmake
```

原因：进入了真正的 CANN 编译门槛，当前环境缺少 Ascend C / CANN 的 CMake 包。

## 在 CANN 环境中的推荐编译步骤

进入装好 CANN Toolkit 的 Linux 或昇腾服务器后，先做环境检查：

```bash
which atc
which npu-smi
find /usr/local/Ascend -name ASCConfig.cmake 2>/dev/null | head
```

如果学校环境要求先加载环境变量，常见形式是：

```bash
source /usr/local/Ascend/ascend-toolkit/set_env.sh
```

然后进入工程目录：

```bash
cd Addcmul_problem_6_template/code
```

重新配置：

```bash
cmake -S . -B build
```

如果仍然找不到 `ASCConfig.cmake`，手动指定 `ASC_DIR`，路径以实际机器上 `find` 的结果为准：

```bash
cmake -S . -B build -DASC_DIR=/usr/local/Ascend/ascend-toolkit/latest/tools/cmake
```

最后编译：

```bash
cmake --build build
```

## 报错时先判断哪一类

| 报错关键词 | 大概率原因 |
| --- | --- |
| `ASCConfig.cmake` | CANN 环境没加载或 `ASC_DIR` 路径不对 |
| `kernel_operator.h` | Ascend C include 路径没进入工程 |
| `DataCopyPad` | CANN 版本 API 签名不一致 |
| `SetDimNum` / `SetDim` | shape 推导接口版本不一致 |
| `half` / `RoundMode` | kernel 类型或 cast API 名称不一致 |
| `tiling` 结构相关 | Host 和 Kernel 侧 tiling 数据定义不匹配 |

遇到第一条真实 CANN 报错后，不要连续乱改。先保留完整报错，再对照关键词定位是哪一层的问题。
