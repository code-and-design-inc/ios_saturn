# Source synchronization map

| Pocket Arcade app source | Public corresponding source |
| --- | --- |
| `../emu/Native/SaturnCore/upstream/yabasanshiro-1.20.37` (extracted archive) | This source root (`yabause/`, `mini18n/`, …) |
| `../emu/Native/SaturnCore/upstream/libchdr` (git clone @ pinned commit) | `libchdr/` (vendored, no `.git`) |
| `../emu/Native/SaturnCore/PocketSaturn` | `PocketSaturn` |
| `../emu/Native/SaturnCore/Support` | `Support` |
| `../emu/Native/SaturnCore/build-ios.sh` | `build-pocket-arcade-ios.sh` (source-root aware; no download step) |
| `../emu/PocketArcade/Saturn/SaturnEmulatorCore.swift` | `PocketArcade/Swift/SaturnEmulatorCore.swift` |
| `../emu/THIRD-PARTY-NOTICES.md` (Yaba Sanshiro section) | `POCKET_ARCADE_SOURCE.md` |

The app checkout's script downloads and checksum-verifies the archive into
`Native/SaturnCore/upstream`; this repository already contains that snapshot.
Review both scripts whenever the build changes. For each distributed app
build, tag the exact public-source commit and update the app's third-party
notices to link to that tag.
