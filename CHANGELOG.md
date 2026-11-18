# Changelog

## Catalog Manager 0.2.0-rc6 · TeoPCE 0.9.15

- Start HuCards and CDs on fresh cards without a previous save or per-game cheat folder. A valid pending CD transaction from RC4 can recover on the console.
- Prepare and install collections on FAT/exFAT, including cards with Mac metadata sidecars. The installer creates the stock game-data directory if needed.
- Includes the first-install and refresh fixes tested in the local RC5 candidate. Existing RC4 users can replace the root ROM without rebuilding their catalog.

## Catalog Manager 0.2.0-rc5 — local release candidate

- A package prepared for a first installation can no longer replace an existing collection. If another install or a play session happened in between, prepare the card again to keep its settings and cheat selections. Incomplete installations are left untouched. A card with only the matching console ROM can still finish setup normally.
- Refreshing a collection in the desktop manager no longer copies the EverDrive's firmware and native saves to each prepared folder. The full-card command-line workflow still carries them forward from the previous card.
- The console program remains **TeoPCE 0.9.13**. There is no firmware update or change to how games launch.

## Catalog Manager 0.2.0-rc4 — first public preview

Bundled console program: **TeoPCE 0.9.13**.

- Original desktop layout with Your card, Collection and Activity sections.
- Clear included/online/offline sources and actual preparation activity.
- Coverage counts, missing-cover filtering, artwork provenance and larger source-image previews.
- Title, year, category, description and custom-cover editing.
- Content-matched licensed cheats: 347 exact releases, 1,177 complete groups.
- Separate preparation and verified installation, with computer-side backup and rollback checks.
- Metadata/prose preservation fixes, legacy placeholder detection and stronger cache/backup/symlink boundaries.
- Standalone macOS arm64 and Windows x64 packages; English/Russian instructions and retained licenses.

The console binary is unchanged from the hardware-confirmed 0.9.13 build, including the fix that allows ordinary CD launching when optional cheat-profile folders are absent. RC4 does not claim new console validation for arbitrary generated collections.

See [RELEASE-STATUS.md](RELEASE-STATUS.md) for evidence and outstanding platform/manual checks. Earlier versions were private development iterations, not public releases.
