# Development and command-line tools

[Project home](../README.md) · [User guide](USER-GUIDE.md)

## Run from source

Use Python 3.10+ with Tk support. A packaged download requires neither Python nor these steps. From the repository root:

```sh
python -m venv .venv
```

Activate it with `source .venv/bin/activate` on macOS, or `.venv\Scripts\Activate.ps1` in Windows PowerShell. If activation is unavailable, invoke the environment's Python executable directly.

```sh
python -m pip install -r requirements.txt
python -m tkinter
python tools/catalog_manager.py
```

The Tk command should display a test window. If not, install a Python/Tk distribution appropriate for your system; a source build needs working Tcl/Tk libraries before packaging.

## Tests

```sh
python -m unittest discover -s tests -v
```

The portable suite uses disposable media fixtures. C-reader integration tests require a C compiler; the optional native build check requires cc65. Tests report skips when those prerequisites are absent. A host fixture test is not a console-play test. Public tests must not depend on personal game collections or absolute owner paths.

## Prepare through the CLI

```sh
python tools/prepare_library.py --help
python tools/prepare_library.py --library /path/to/games --output /path/to/new-output
```

The default mode copies supported games into `new-output/card/` with the matching TeoPCE files. Output must be a fresh path, separate from the source. Review `REPORT.json`, `INSTALL.txt` and `SHA256SUMS.txt` in the output. Copy the card generation as a whole onto a separately backed-up card with the native EverDrive setup. Do not merge it into an existing installation without preserving state.

For games already on the destination card:

```sh
python tools/prepare_library.py --library /path/to/card --output /path/to/new-output --data-only
```

`--data-only` omits game copies. For normal installation use the manager, which also verifies backups and card state. `--previous` supplies a complete previous prepared card for identity/state preservation; `--reference` supplies your owned card for exact-content metadata/art/cheat reuse. `--offline` avoids downloads. `--cheat-database` selects an additional version-1 TeoPCE JSON library. `--metadata` selects a version-1 JSON document with a `games` mapping keyed by relative game path.

Example metadata sidecar:

```json
{
  "version": 1,
  "games": {
    "HuCard/Example.pce": {
      "title": "Example",
      "year": 1991,
      "category": "original",
      "description": "Your own short description.",
      "cover": "/path/to/your-cover.png"
    }
  }
}
```

Categories: `original`, `translations`, `homebrew`, `museum`, `tools`, `unlicensed`. Use year `0` for unknown. Paths above are examples; keep them separate from source/output/cache. Revisions, renamed games and previous saved state require explicit identity handling.

Cheat schema, pinned source reconstruction and export: [CHEAT-LIBRARY.md](CHEAT-LIBRARY.md).

## Package the desktop manager

Build on each target operating system; this is not a cross-compiler:

```sh
python -m pip install -r requirements-build.txt
python tools/build_manager.py --resources resources --output dist
```

The output contains the current host's app or EXE, bundled runtime, ROM and resource database. Include README, docs, LICENSE, THIRD-PARTY, RELEASE-STATUS, VERIFICATION and the licenses directory beside the app when distributing it. Preserve macOS bundle symlinks/permissions when archiving. Validate the extracted application, not only the build directory.

The macOS application uses an ad-hoc signature, not notarization. The Windows application is unsigned. The release workflow builds host artifacts and runs startup smoke checks; it does not automatically publish a GitHub release or provide trusted signing.

## Optional native ROM build

The repository includes the hardware-confirmed TeoPCE 0.9.13 ROM. You do not need cc65 for desktop use or card preparation.

For native development, install cc65 with its headers and `lib/nes.lib`:

```sh
python tools/build_rom.py --cc65-bin /path/to/cc65/bin --native-only --app-root 00TEOPCE --output-dir build/native
```

This builds the 128 KiB console ROM independently of private preview assets. The historical compiler commit for the bundled ROM was not recorded, so byte-identical reproduction has not been demonstrated. A rebuilt ROM is a developer build, not automatically equivalent to hardware validation of the distributed binary. The manager intentionally verifies the bundled release ROM hash.

## Repository map

| Path | Responsibility |
| --- | --- |
| `rom/` | Console UI, catalog readers, launch bridge, cheat/save handling |
| `tools/catalog_manager.py` | Desktop state, actions and workers |
| `tools/catalog_manager_view.py` | Desktop view/layout |
| `tools/prepare_library.py` | Prepare a matching catalog generation |
| `tools/library_media.py` | Input scanning, fingerprints and media validation |
| `tools/library_install.py` | Verified backup, staging, installation and rollback |
| `resources/` | Release ROM, pinned metadata and attributed cheat library |
| `tests/` | Portable regressions and production-reader integration |
| `licenses/` | Dependency and data notices |

See [CONTRIBUTING.md](../CONTRIBUTING.md) before submitting a patch. Preserve stock EverDrive behaviour and treat paths, identity, profiles, full cheat groups and rollback as invariants.
