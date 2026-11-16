"""The public source can build the console ROM without private preview assets."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOLCHAIN = Path(os.environ.get("TEOPCE_CC65_BIN", "/tmp/teo-cc65/bin"))


class PublicNativeBuildTests(unittest.TestCase):
    @unittest.skipUnless((TOOLCHAIN / "cc65").is_file(), "Optional cc65 toolchain unavailable")
    def test_native_only_build_avoids_private_preview_inputs(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td) / "native"
            result = subprocess.run([sys.executable, "-B", str(ROOT / "tools/build_rom.py"),
                                     "--cc65-bin", str(TOOLCHAIN), "--app-root", "00TEOPCE",
                                     "--output-dir", str(output), "--native-only"],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((output / "TeoPCE.pce").stat().st_size, 131072)
            self.assertFalse((output / "preview").exists())
            self.assertFalse((output / "probe").exists())


if __name__ == "__main__":
    unittest.main()
