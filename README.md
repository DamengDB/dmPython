# dmPython

dmPython 是达梦数据库（DM8）的原生 Python 驱动程序，遵循 [Python DB API 2.0](https://www.python.org/dev/peps/pep-0249/) 规范。通过 C 扩展模块和 DPI（Dameng Programming Interface）库与达梦数据库通信。

**当前版本：** 2.5.30

## 特性

- 遵循 Python DB API 2.0 规范
- 支持 Python 2.7 及 Python 3.x（含 3.12+）
- 支持 SQLAlchemy 和 Django 框架
- 支持 BLOB/CLOB、BFILE、对象类型等丰富的数据类型
- 支持元组游标和字典游标两种模式
- 跨平台：Linux、Windows、macOS

## 前置条件

1. 已安装达梦数据库或拥有达梦驱动目录（drivers）
2. 设置 `DM_HOME` 环境变量，指向达梦安装目录或 drivers 目录，该路径下必须有 `include` 或 `dpi/include` 目录

```bash
# Linux / macOS
export DM_HOME=/opt/dmdbms

# Windows (PowerShell)
$env:DM_HOME = "C:\dmdbms"
```

## 安装

### 源码安装（推荐，跨平台通用）

```bash
cd dmPython
python setup.py install
```

### 生成平台安装包

```bash
# Linux RPM
python setup.py bdist_rpm

# Windows
python setup.py bdist_wininst
```

安装 RPM 包：

```bash
rpm -ivh dist/dmPython-*.rpm --nodeps
```

卸载 RPM 包：

```bash
rpm -e dmPython
```

## 运行时配置

dmPython 运行时依赖 DPI 动态库，需确保系统能找到该库文件：

| 平台    | 库文件            | 配置方式                                      |
| ------- | ----------------- | --------------------------------------------- |
| Linux   | `libdmdpi.so`     | `export LD_LIBRARY_PATH=/opt/dmdbms/bin`      |
| Windows | `dmdpi.dll`       | 将 DPI 所在目录添加到 `PATH` 环境变量         |
| macOS   | `libdmdpi.dylib`  | `export LD_LIBRARY_PATH=/opt/dmdbms/bin`      |

## 快速开始

```python
import dmPython

# 建立连接
conn = dmPython.connect(user='SYSDBA', password='SYSDBA001', server='localhost', port=5236)

# 创建游标并执行 SQL
cursor = conn.cursor()
cursor.execute("SELECT * FROM SYSOBJECTS WHERE ROWNUM <= 5")

# 获取结果
rows = cursor.fetchall()
for row in rows:
    print(row)

# 关闭连接
cursor.close()
conn.close()
```

## 构建与测试

```bash
# 仅编译（不安装）
python setup.py build

# 运行测试
python setup.py test
```

### 调试追踪

在 `setup.py` 中取消以下注释，重新编译后会在当前目录生成 `dmPython_trace.log`：

```python
defineMacros.append(('TRACE', None))
```

### 达梦版本

默认针对 DM 8.1 构建，可通过环境变量覆盖：

```bash
export DM_VER=8.2
```

## 常见问题

### Windows: `Unable to find vcvarsall.bat`

进入 Python 安装目录 `Lib/distutils/msvc9compiler.py`，找到：

```python
vc_env = query_vcvarsall(VERSION, plat_spec)
```

将 `VERSION` 替换为本机安装的 Visual Studio 版本号，例如：

```python
vc_env = query_vcvarsall(10, plat_spec)
```

### `ImportError: DLL load failed: 找不到指定的模块`

dmPython 无法找到 DPI 动态库。请按上方「运行时配置」章节配置环境变量，使系统能定位到 `dmdpi.dll`（Windows）或 `libdmdpi.so`（Linux）。

### `undefined symbol: PyUnicodeUCS2_Format`

编译 dmPython 的环境 UCS 编码与运行环境不匹配。常见原因：

1. **跨平台编译**：在不同操作系统上编译和使用 dmPython。解决方法：在同一台机器上编译和使用。
2. **Python 源码安装时编码不一致**：使用源码安装 Python 时，确保 `--enable-unicode` 选项与操作系统一致：

```bash
./configure --prefix=$YOUR_PATH --enable-unicode=ucs4
```

## 项目结构

```
dmPython/
├── setup.py          # 构建脚本
├── py_Dameng.c/h     # 模块入口，类型注册，异常层次
├── strct.h           # 核心结构定义 (Environment, Connection, Cursor)
├── Connection.c      # 连接管理
├── Cursor.c          # 游标操作，SQL 执行
├── row.c/h           # 结果行对象
├── Environment.c     # 环境管理
├── Error.c           # 错误处理
├── Buffer.c          # 缓冲区管理
├── var.c             # 变量管理核心
├── var_pub.h         # 变量类型接口定义
├── vString.c         # 字符串类型
├── vNumber.c         # 数值类型
├── vDateTime.c       # 日期时间类型
├── vInterval.c       # 间隔类型
├── vLob.c            # BLOB/CLOB
├── vBfile.c          # BFILE
├── vCursor.c         # 嵌套游标
├── vObject.c         # 对象类型
├── vlong.c           # 长数据类型
├── exLob.c           # 外部 LOB 接口
├── exObject.c        # 外部对象接口
├── exBfile.c         # 外部 BFILE 接口
├── tObject.c         # 对象类型元数据
└── trc.c             # 调试追踪
```

## 许可证

Python Software Foundation License
