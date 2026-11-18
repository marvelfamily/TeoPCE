#!/usr/bin/env python3
"""Package the desktop manager and its verified ROM using PyInstaller."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--resources", type=Path, required=True,
                     help="Resource directory containing the verified ROM and database-cache")
    cli.add_argument("--output", type=Path, required=True)
    args = cli.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    resources = args.resources.resolve(strict=True)
    from prepare_library import ROM_SHA256, ROM_VERSION
    import hashlib
    if hashlib.sha256((resources / "TeoPCE.pce").read_bytes()).hexdigest() != ROM_SHA256:
        raise ValueError(f"Use the verified {ROM_VERSION} ROM")
    modules = ["catalog_manager_view", "prepare_library", "library_media", "library_sources", "library_install", "library_cheats",
               "build_catalog", "build_index", "build_find_index", "build_launch_metadata",
               "build_game_metadata", "build_cheat_index", "prepare_cheat_groups",
               "build_cheat_groups", "build_cheat_view", "artwork", "convert_art",
               "enrich_genres", "descriptions"]
    with tempfile.TemporaryDirectory(prefix="teopce-manager-build-") as td:
        temp = Path(td)
        from PIL import Image, ImageDraw
        icon_image = Image.new("RGB", (1024, 1024), "#f4f1eb")
        drawing = ImageDraw.Draw(icon_image)
        drawing.rounded_rectangle((215, 150, 809, 874), radius=65, fill="#20343e")
        drawing.rounded_rectangle((275, 210, 749, 580), radius=22, fill="#e8aa65")
        drawing.rectangle((315, 650, 405, 874), fill="#f4f1eb")
        drawing.rectangle((619, 650, 709, 874), fill="#f4f1eb")
        icon = temp / ("TeoPCE.icns" if sys.platform == "darwin" else "TeoPCE.ico")
        icon_image.save(icon)
        command = [sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--onefile" if sys.platform == "win32" else "--onedir",
                   "--windowed", "--icon", str(icon), "--name", "TeoPCE Catalog Manager", "--paths", str(ROOT / "tools"),
                   "--distpath", str(output), "--workpath", str(temp / "work"), "--specpath", str(temp),
                   "--add-data", str(resources) + ":resources"]
        for name in modules:
            command += ["--hidden-import", name, "--add-data", str(ROOT / "tools" / (name + ".py")) + ":."]
        if sys.platform == "darwin":
            command += ["--osx-bundle-identifier", "org.teopce.catalogmanager"]
        subprocess.run(command + [str(ROOT / "tools/catalog_manager.py")], check=True)
    if sys.platform == "darwin":
        import plistlib
        path = output / "TeoPCE Catalog Manager.app/Contents/Info.plist"
        with path.open("rb") as stream:
            document = plistlib.load(stream)
        document.update(CFBundleShortVersionString="0.2.0", CFBundleVersion="7")
        with path.open("wb") as stream:
            plistlib.dump(document, stream)
        subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(path.parents[1])], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
