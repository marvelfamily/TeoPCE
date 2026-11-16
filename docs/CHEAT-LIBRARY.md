# Cheat libraries

Preparation automatically matches the bundled library against the scanned game bytes. The console remains responsible for choosing and enabling groups. Import extra cheats accepts another JSON library. A conflicting entry for identical content stops preparation and reports the conflict. Existing installed packages retain their bytes and selections.

A library has version 1 and an entries list. Each entry has title, kind (1 HuCard or 2 CD), path (relative ASCII game path), content_identity (lowercase SHA-256), size, crc32 (integer), requires_path (boolean), sources (HTTPS links) and groups.

Each group has id (unsigned 32-bit), name (printable ASCII), reason (existing TCG held flags) and codes. Each code is a list of address, byte value and domain. Domain 1 is a supported ROM address, domain 2 a supported RAM address. CD active groups require RAM writes. Group IDs and order are part of saved selection identity. Use the production encoder and decoder in tools/build_cheat_groups.py to validate authoring.

For HuCard, content_identity is SHA-256 of the complete file. For CD it is SHA-256 of the exact CUE bytes, a zero byte and the concatenation of each distinct referenced track's binary SHA-256 in first-reference order. Every track participates, including audio. Cosmetic CUE changes require a new binding. A source describing only a game's name cannot establish an exact revision match.

The export tool reads a complete owned reference and its game library.

```sh
python tools/export_cheat_library.py --reference /path/to/reference-card --library /path/to/games --output new-library.json
```

The optional verified-manifest input is an explicit authority for a previously verified reference snapshot. It exports that recorded full-track identity and checks the exact CUE against the snapshot. Default export reads and compares the complete reference games. Its report describes hash provenance. Prepare_library also accepts --cheat-database for command-line use.

53 bundled CD bindings require their recorded path because the 0.9.13 runtime owns compiled notes and guarded startup. The manager displays the required path when a content match cannot be attached there. Using an alternative path requires a separately validated runtime-policy migration.

The bundled library is adapted from pinned Libretro data under CC BY-SA 4.0. Source attribution and adaptation details are recorded in THIRD-PARTY.md. The MIT code license applies to TeoPCE-owned code. Explicit user imports retain their own terms.

## Bundled data provenance

The public library contains 347 exact releases and 1,177 groups adapted from a pinned Libretro snapshot under CC BY-SA 4.0. Original files, attribution and hashes are retained in resources/cheats/. This data subset preserves complete matching address/value sequences and the reference's group IDs, reasons and CD path constraints. An explicit imported library takes priority over a bundled record for the same content. Installed packs and selections are preserved during repeated preparation.

Run `python tools/import_libretro_cheats.py --reference-library resources/cheats/library.json --upstream resources/cheats/upstream --manifest resources/cheats/SOURCES.json --output build/reconstructed-cheats.json` to reconstruct the included library from its pinned source evidence. The output must use a fresh path.
