"""Setup script for dmPython-macOS.

Builds the dmPython C extension with the Go-based DPI bridge library,
producing a self-contained wheel that requires no DM_HOME or Go toolchain.

    python -m build --wheel
    # or
    python setup.py build_ext --inplace
"""
import os
import struct
import subprocess
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext as _build_ext

BUILD_VERSION = "2.5.30"

# All C source files
C_SOURCES = [
    "py_Dameng.c",
    "row.c",
    "Cursor.c",
    "Connection.c",
    "Environment.c",
    "Error.c",
    "Buffer.c",
    "exLob.c",
    "exObject.c",
    "tObject.c",
    "var.c",
    "vCursor.c",
    "vDateTime.c",
    "vInterval.c",
    "vLob.c",
    "vNumber.c",
    "vObject.c",
    "vString.c",
    "vlong.c",
    "exBfile.c",
    "vBfile.c",
    "trc.c",
]

# Directories
HERE = os.path.dirname(os.path.abspath(__file__))
DPI_BRIDGE_DIR = os.path.join(HERE, "dpi_bridge")


def find_dpi_include():
    """按优先级查找 DPI 头文件目录。"""
    candidates = [
        os.path.join(HERE, "dpi_include"),
    ]
    dm_home = os.environ.get("DM_HOME")
    if dm_home:
        candidates.append(os.path.join(dm_home, "include"))
        candidates.append(os.path.join(dm_home, "dpi", "include"))
    for d in candidates:
        if os.path.isfile(os.path.join(d, "DPI.h")):
            return d
    raise RuntimeError(
        "Cannot find DPI header files (DPI.h). Options:\n"
        "  1. Place headers in ./dpi_include/\n"
        "  2. Set DM_HOME to your Dameng installation directory\n"
        "See README.md for details."
    )


DPI_INCLUDE_DIR = find_dpi_include()

# Macros
define_macros = []
if struct.calcsize("P") == 8:
    define_macros.append(("DM64", None))
if sys.platform == "win32":
    define_macros.append(("WIN32", None))
    define_macros.append(("_CRT_SECURE_NO_WARNINGS", None))
# Uncomment for debug tracing:
# define_macros.append(("TRACE", None))


class build_ext(_build_ext):
    """Custom build_ext that builds the Go bridge library before compiling."""

    def run(self):
        if not os.environ.get("DMPYTHON_SKIP_GO_BUILD"):
            self._build_go_bridge()
        super().run()

    def _build_go_bridge(self):
        dylib_path = os.path.join(DPI_BRIDGE_DIR, "libdmdpi.dylib")

        # Check if Go is available
        try:
            subprocess.check_output(["go", "version"], stderr=subprocess.STDOUT)
        except (FileNotFoundError, subprocess.CalledProcessError):
            if os.path.exists(dylib_path):
                print("Go not found, using pre-built libdmdpi.dylib")
                return
            raise RuntimeError(
                "Go toolchain is required to build the DPI bridge library. "
                "Install Go from https://go.dev/dl/ or set DMPYTHON_SKIP_GO_BUILD=1 "
                "if you have a pre-built libdmdpi.dylib in dpi_bridge/"
            )

        print("Building Go DPI bridge library...")
        subprocess.check_call(
            [
                "go", "build",
                "-buildmode=c-shared",
                "-o", "libdmdpi.dylib",
                ".",
            ],
            cwd=DPI_BRIDGE_DIR,
        )

        # Set install_name so delocate and @rpath work correctly
        subprocess.check_call(
            [
                "install_name_tool",
                "-id", "@rpath/libdmdpi.dylib",
                dylib_path,
            ]
        )
        print("Go DPI bridge library built successfully.")


extension = Extension(
    name="dmPython",
    sources=C_SOURCES,
    include_dirs=[DPI_INCLUDE_DIR],
    library_dirs=[DPI_BRIDGE_DIR],
    libraries=["dmdpi"],
    define_macros=define_macros,
    extra_compile_args=["-DBUILD_VERSION=%s" % BUILD_VERSION],
    extra_link_args=["-Wl,-rpath,@loader_path"],
)

setup(
    name="dmPython-macOS",
    version=BUILD_VERSION,
    description="Python DB-API 2.0 driver for DM (Dameng) database — macOS edition with built-in Go bridge",
    long_description=open(os.path.join(HERE, "README.md"), encoding="utf-8").read()
    if os.path.exists(os.path.join(HERE, "README.md"))
    else "",
    long_description_content_type="text/markdown",
    ext_modules=[extension],
    cmdclass={"build_ext": build_ext},
    keywords="Dameng DM8 database DB-API",
    license="MulanPSL-2.0",
    python_requires=">=3.8",
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Natural Language :: English",
        "Operating System :: MacOS :: MacOS X",
        "Programming Language :: C",
        "Programming Language :: Python :: 3",
        "Topic :: Database",
    ],
)
