"""Build/load integration checks with a caller-provided macOS DPI SDK."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SDK = os.environ.get("DMPYTHON_TEST_SDK")


@unittest.skipUnless(sys.platform == "darwin" and SDK,
                     "requires macOS and DMPYTHON_TEST_SDK with native DPI libraries and headers")
class MacOSDpiRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="dmpython-runtime-")
        cls.addClassCleanup(cls.temporary.cleanup)
        directory = Path(cls.temporary.name).resolve()
        # Exercise argument forwarding with spaces and commas, not shell quoting.
        home = directory / "DPI SDK, runtime"
        library_directory = home / "bin"
        library_directory.mkdir(parents=True)
        source = Path(SDK).resolve()
        library = next((p for p in (source / "bin" / "libdmdpi.dylib",
                                   source / "dpi" / "libdmdpi.dylib",
                                   source / "libdmdpi.dylib") if p.is_file()), None)
        headers = next((p for p in (source / "include", source / "dpi" / "include")
                        if (p / "DPI.h").is_file()), None)
        if library is None or headers is None:
            raise RuntimeError("DMPYTHON_TEST_SDK must contain libdmdpi.dylib and DPI.h")
        target = library_directory / "libdmdpi.dylib"
        shutil.copy2(library, target)
        shutil.copytree(headers, home / "include")
        subprocess.run(["install_name_tool", "-id", "@rpath/libdmdpi.dylib", str(target)],
                       check=True, capture_output=True, encoding="utf-8")
        cls.environment = {key: value for key, value in os.environ.items()
                           if not key.startswith("DYLD_") and key != "PYTHONPATH"}
        cls.environment["DM_HOME"] = str(home)
        cls.library_directory = library_directory
        cls.build_library = directory / "extension"
        build = subprocess.run(
            [sys.executable, "setup.py", "build_ext", "--force", "--build-temp",
             str(directory / "objects"), "--build-lib", str(cls.build_library)],
            cwd=ROOT, env=cls.environment, capture_output=True, encoding="utf-8",
        )
        if build.returncode:
            raise RuntimeError("DPI integration build failed:\n" + build.stdout + build.stderr)

    def test_import_without_dyld_environment(self):
        program = (
            "import json,sys; sys.path.insert(0," + repr(str(self.build_library)) + "); "
            "import dmPython; print(json.dumps({'module':dmPython.__file__,"
            "'version':dmPython.version}))"
        )
        result = subprocess.run([sys.executable, "-I", "-c", program],
                                cwd=self.temporary.name, env=self.environment,
                                capture_output=True, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stderr)
        loaded = json.loads(result.stdout)
        self.assertEqual(Path(loaded["module"]).resolve().parent, self.build_library)
        self.assertTrue(loaded["version"])

    def test_extension_records_sdk_runpath(self):
        extension, = self.build_library.glob("dmPython*.so")
        result = subprocess.run(["otool", "-l", str(extension)], check=True,
                                capture_output=True, encoding="utf-8")
        self.assertIn("path " + str(self.library_directory) + " (offset", result.stdout)


if __name__ == "__main__":
    unittest.main()
