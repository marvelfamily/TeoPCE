# Catalog Manager 0.2.0-rc4

A public release candidate for macOS Apple Silicon and Windows x64. The console ROM remains the verified TeoPCE 0.9.13 binary.

## Changes

An original TeoPCE desktop layout separates Your card, Collection and Activity. Setup explains included facts and cheats, online artwork and offline operation. Collection adds coverage counts, missing-cover filtering, source labels and original-image previews. Activity reports actual scan, cache and download operations. Preparation and installation remain separate; unsaved edits prevent a stale installation.

Independent QA fixes preserve unspecified metadata during partial edits, preserve prose separately from publisher/developer facts, identify both legacy placeholder encodings, bind desktop previews to their prepared image digest, and reject unsafe cache/backup paths and symbolic links before promotion. Older cards without structured prose require an explicit description when changing developer/publisher through a metadata sidecar; their existing text is not silently replaced.

## Verification

The final clean Mac source passed 42 tests. Windows passed 37 with 5 skips: two C-reader checks, the optional cc65 build and two symlink-creation tests unavailable in that guest. An automated manager workflow prepared a collection, edited a game, prepared again and installed it into a disposable folder with a verified backup and unchanged game bytes. Backend checks cover failure/rollback and preserved profiles/native saves.

The broad working-project suite ran 445 tests: one historical skip and one 3-second timeout in an unchanged native fixture while other builds were running. The affected four-test suite then passed separately. No native test timeout was hidden by increasing its limit.

Mac setup and collection screens were visually inspected, including the minimum supported window size. Mac and Windows frozen applications passed startup checks. macOS is ad-hoc signed and strictly verified. Windows was tested using x64 emulation on Windows 11 ARM64 in Parallels.

A full manual preparation/install pass through the packaged interfaces remains pending; automated native input did not reliably exercise that flow. Source controller operations were exercised with real workers and disposable files. A new arbitrary collection still requires console validation. Native Intel Windows, Intel Mac, Apple notarization, Windows Authenticode signing and the GitHub CI workflow remain unverified. Release checksums identify the distributed artifacts.

## Data and licenses

TeoPCE code is MIT. The bundled Libretro-derived cheat library is CC BY-SA 4.0: 347 exact releases and 1,177 complete groups. Existing installed packs and selections keep priority. Full prose descriptions are supplied by the user or an owned reference, not downloaded automatically. Attribution and license files are included. No games or BIOS files are distributed.
