# dmpython

该仓库主要提供了支持 Python 连接达梦数据库的 Python 库。

## 主要功能

支持使用 Python 连接达梦数据库，通过 Python 接口操作数据库，进行创表、删表、数据增删改查等功能，并展示数据内容。

## 使用方法

dmPython 源码依赖 DM 安装目录或者 drivers 目录（drivers 目录为 DM 的驱动目录）中提供的 dpi 头文件。

安装前需要检查环境中是否存在 DM 安装目录或 drivers 目录，并设置 `DM_HOME`：

```bash
export DM_HOME=/opt/dmdbms
# 或
export DM_HOME=/drivers
```

具体路径以实际环境为准，`DM_HOME` 路径下必须有 `include` 目录或者 `dpi/include` 目录。

### Windows 安装

在 Windows 操作系统下安装 dmPython，只需要直接执行 exe 文件即可。

Windows 操作系统下生成 exe 文件操作如下：

1. 进入到 dmPython 源码所在目录（`setup.py` 文件所在路径）。
2. 执行命令：

```bash
python setup.py bdist_wininst
```

3. 在 `dist` 目录下会生成相关安装文件。

### Linux 安装

1. 进入到 dmPython 源码所在目录（`setup.py` 文件所在路径）。
2. 执行命令：

```bash
python setup.py bdist_rpm
```

3. 在 `dist` 目录下会生成相关 rpm 包。
4. 在 Linux 操作系统下使用 rpm 包安装 dmPython。安装和卸载命令参考如下：

安装：

```bash
rpm -ivh dmPython-2.1-7.1-py33-1.x86_64.rpm --nodeps
```

卸载：

```bash
rpm -e dmPython-2.1-1.x86_64
```

### 源码安装（Windows/Linux）

Windows 和 Linux 也可以直接使用源码安装，操作如下：

1. 进入到 dmPython 源码所在目录（`setup.py` 文件所在路径）。
2. 执行命令：

```bash
python setup.py install
```

## 其他可能有帮助的信息

### 生成安装包

Windows 平台生成安装包（exe）：

```bash
python setup.py bdist_wininst
```

Linux 平台生成安装包（rpm）：

```bash
python setup.py bdist_rpm
```

### 依赖问题处理

若安装过程中出现依赖问题，则：

```bash
rpm -ivh file.rpm --nodeps
```

Linux 安装命令：

```bash
rpm -ivh dmPython-1.1-7.1-py26-1.x86_64.rpm --nodeps
```

Linux 卸载命令：

```bash
rpm -e dmPython-1.1-1.x86_64
```

源码直接安装（不分平台）：

```bash
python setup.py install
```

### 64 位平台安装

64 位平台安装时，需增加 `DM64` 宏：

- 安装脚本 `setup.py` 中全局变量 `defineMacros` 使用 `defineMacros = [('DM64', None),]`。
- 否则，使用 `defineMacros = []`。

平台执行上述命令时，需先进行如下准备工作：定义环境变量 `DM_HOME`。

- Windows 平台需要将其添加到环境变量 `PATH` 中。
- Linux 平台则不需要。

指定为 DM 安装目录 `bin` 的上层目录，例如：

```bash
# Windows
DM_HOME=C:\dmdbms

# Linux
export DM_HOME=/opt/dmdbms
```

## 常见问题

### WIN 平台可能遇到：Unable to find vcvarsall.bat

解决方案如下：

1. 进入当前使用 Python 安装目录中的 `Lib/distutils`，找到文件 `msvc9compiler.py`。
2. 使用 UE 或其他文本编辑器打开 `msvc9compiler.py`。
3. 在文件中找到：

```python
vc_env = query_vcvarsall(VSERSION, plat_spec)
```

4. 根据本机安装的 VS 版本号，对应安装目录（如：`C:\Program Files\Microsoft Visual Studio 10.0`），修改为：

```python
vc_env = query_vcvarsall(10, plat_spec)
```

### WIN 平台执行 `import dmPython` 时可能遇到 ImportError

示例报错：

```text
import dmPython
Traceback (most recent call last):
  File "", line 1, in
ImportError: DLL load failed: 找不到指定的模块
```

此时因为 dmPython 找不到动态库 dpi（Linux 为 `libdmdpi.so`，Windows 为 `dmdpi.dll`、`dmdpi.lib`），需要到 dpi 所在目录执行或者配置环境变量指向 dpi 所在目录。

若有安装 DM，直接配置环境变量指向 `bin` 目录或者指向 `drivers/dpi`。

Linux 示例：

```bash
export LD_LIBRARY_PATH=/opt/dmdbms/bin
# 或
export LD_LIBRARY_PATH=/drivers/dpi
```

### Python 使用过程中遇到 `undefined symbol: PyUnicodeUCS2_Format`

此问题为编译 dmPython 的环境 UCS 编码与执行环境不匹配导致，常见情况：

1. 在不同的操作系统环境中编译和使用 dmPython。
2. 编译或安装 dmPython 的 Python 程序本身 UCS 编码与当前操作系统不一致。

这两种情况都与 dmPython 源码无关，检查当前环境即可。

解决：

- 第一种：在同一台机子上编译和使用即可。
- 第二种：一般是使用源码安装了 Python，然后再用 Python 去编译或安装 dmPython，因此需要检查在使用源码安装 Python 时使用的编码与操作系统是否一致。

源码安装 Python 参考命令如下：

```bash
./configure --prefix=$YOUR_PATH --enable-unicode=ucs4
```

`--enable-unicode` 选项指定成与操作系统一致的编码即可。
