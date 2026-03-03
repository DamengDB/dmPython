# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

dmPython 是达梦数据库（DM8）的原生 Python 驱动，遵循 Python DB API 2.0 规范。整个项目是一个 C 扩展模块，通过 DPI（Dameng Programming Interface）库与达梦数据库通信。当前版本 2.5.30，支持 Python 2.7 及 3.x（含 3.12+），支持 sqlalchemy 和 django 框架。

## 构建与安装

**前置条件：** 需设置 `DM_HOME` 环境变量指向达梦安装目录或 drivers 目录，该路径下必须有 `include` 或 `dpi/include` 目录。

```bash
# 设置环境变量
export DM_HOME=/opt/dmdbms  # 或 /drivers，具体以实际环境为准

# 源码安装
python setup.py install

# 仅编译
python setup.py build

# 运行测试（需要 test/ 目录下有 test.py/test3k.py）
python setup.py test

# 生成平台安装包
python setup.py bdist_rpm       # Linux RPM
python setup.py bdist_wininst   # Windows
```

**运行时依赖：** 需要 DPI 动态库（Linux: `libdmdpi.so`，Windows: `dmdpi.dll`，macOS: `libdmdpi`），通过 `LD_LIBRARY_PATH` 或 `PATH` 指向 DPI 所在目录。

**达梦版本：** 默认 8.1，可通过 `DM_VER` 环境变量覆盖。

**调试追踪：** 取消 `setup.py` 中 `defineMacros.append(('TRACE', None))` 的注释即可在当前目录生成 `dmPython_trace.log`。

## 代码架构

所有源码（C/H 文件）位于项目根目录，无子目录结构。

### 核心层次

1. **模块入口** — `py_Dameng.c` / `py_Dameng.h`
   - Python 模块初始化，注册所有类型对象和 DB API 2.0 异常层次（Warning, Error, InterfaceError, DatabaseError 等）

2. **核心结构定义** — `strct.h`
   - `dm_Environment`：DPI 环境句柄、编码信息
   - `dm_Connection`：连接句柄、连接参数、事务设置、类型处理器
   - `dm_Cursor`：语句句柄、列/参数描述符、变量绑定、行计数器

3. **连接管理** — `Connection.c`
   - 连接建立/关闭、属性读写（autocommit、timeout、隔离级别等）
   - 支持 shutdown 模式：ABORT、IMMEDIATE、TRANSACTIONAL、NORMAL

4. **游标操作** — `Cursor.c`（最大文件）
   - SQL 执行、参数绑定、结果集获取、批量操作
   - 支持 TUPLE_CURSOR (0) 和 DICT_CURSOR (1) 两种模式

5. **行对象** — `row.c` / `row.h`
   - 类元组的结果行，支持列名到索引的映射

6. **环境/错误/缓冲** — `Environment.c`, `Error.c`, `Buffer.c`

### 变量类型系统

核心接口定义在 `var_pub.h`，类型调度通过 `dm_VarType` 结构体中的函数指针表实现（initializeProc, finalizeProc, setValueProc, getValueProc 等）。

- `var.c` — 变量管理和类型选择的核心逻辑
- `vString.c` — 字符串/varchar 处理
- `vNumber.c` — 数值类型转换（integer, bigint, float, double, decimal）
- `vDateTime.c` — 日期、时间、时间戳、时区
- `vInterval.c` — 间隔类型
- `vLob.c` — BLOB/CLOB 读写
- `vBfile.c` — BFILE（外部文件）
- `vCursor.c` — 嵌套游标
- `vObject.c` — 对象和记录类型
- `vlong.c` — 长二进制/长字符串

### 外部对象封装

- `exLob.c` — 外部 LOB 接口（读写/截断操作）
- `exObject.c` — 外部对象接口（属性访问）
- `exBfile.c` — 外部 BFILE 接口
- `tObject.c` — 对象类型元数据
- `trc.c` — 调试追踪工具

## 关键常量

```c
#define MAX_STRING_CHARS  4094
#define MAX_BINARY_BYTES  8188
#define NAMELEN           128
#define PY_SQL_MAX_LEN    0x8000
```

## 编码注意事项

- 64 位平台自动定义 `DM64` 宏（`setup.py` 中根据 `struct.calcsize("P")` 判断）
- Windows 平台定义 `WIN32` 和 `_CRT_SECURE_NO_WARNINGS`
- 构建 dmPython 的环境 UCS 编码需与运行环境一致，否则会出现 `undefined symbol: PyUnicodeUCS2_Format` 错误
- Python 3.12+ 使用 setuptools 替代 distutils
