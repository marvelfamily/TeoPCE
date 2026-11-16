#!/usr/bin/env python3
"""Create the builder's local Python environment and run preparation."""
from pathlib import Path
import subprocess
import sys
import venv

if sys.version_info < (3, 10):
    print("Install Python 3.10 or newer from python.org", file=sys.stderr)
    raise SystemExit(1)

root = Path(__file__).resolve().parents[1]
environment = root / ".python-env"
python = environment / ("Scripts/python.exe" if sys.platform == "win32" else "bin/python")
try:
    if not python.is_file():
        print("Preparing the local Python environment...", flush=True)
        venv.EnvBuilder(with_pip=True).create(environment)
    result = subprocess.run([str(python), "-c", "from PIL import Image"] , capture_output=True)
    if result.returncode:
        subprocess.run([str(python), "-m", "pip", "install", "--disable-pip-version-check",
                        "Pillow>=10,<13"], check=True)
    result = subprocess.run([str(python), "-B", str(root / "tools/prepare_library.py"), *sys.argv[1:]])
    raise SystemExit(result.returncode)
except (OSError, subprocess.CalledProcessError) as error:
    print("Setup failed - " + str(error), file=sys.stderr)
    raise SystemExit(1)
