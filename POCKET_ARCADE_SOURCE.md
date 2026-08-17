# Pocket Arcade Sega Saturn source release

This repository is the complete corresponding source for Pocket Arcade's Sega
Saturn core. It is based on the Yaba Sanshiro 1.20.37 corresponding-source
archive published by the project (`yabasanshiro-src-1.20.37.tar.gz`, SHA-256
`d28b98d725771fd7f52e2db11b247bcbe0974f9d648731006159c017b669a338`,
https://www.yabasanshiro.com/download). The repository's first commit is an
exact snapshot of that archive minus prebuilt third-party binary libraries
that are not part of this build (`yabause/src/vulkan/lib`, `yabause/src/qt/lib`,
`yabause/src/glfw/lib`, `MetalANGLE.framework/.xcframework`). Pocket Arcade's
port is committed on top and does not modify any upstream file.

The port consists of:

- `PocketSaturn/` — the C ABI (`PASaturnBridge.h`) and its implementation,
  which is the entire front-end ("yui") layer the core links against: core
  lists, a lock-free ring-buffer sound core, `YuiSwapBuffers` frame capture,
  and stubs for features that are not compiled (OpenGL renderer entry points,
  RetroAchievements, PlayRecorder). Plus the CMake project that selects the
  upstream files (SH-2 interpreter, Musashi 68000, SCSP, VIDSoft software
  renderer, ISO/CUE/CHD) and the headless macOS harness.
- `libchdr/` — the pinned libchdr fork (`devmiyax/libchdr` @
  `5a642352731a5abb1322bf0749b0e1822ebb393a`, BSD-3-Clause) with its bundled
  lzma and zstd sources.
- `Support/` — framework Info.plist.
- `PocketArcade/Swift/SaturnEmulatorCore.swift` — the app-side adapter.
- `build-pocket-arcade-ios.sh` — the reproducible build recipe.

## Clone and build

```sh
git clone https://github.com/code-and-design-inc/ios_saturn.git
cd ios_saturn
git checkout pocket-arcade-1.0-source
./build-pocket-arcade-ios.sh            # dist/SaturnCore.xcframework
./build-pocket-arcade-ios.sh --headless # + build/macos-headless/saturnheadless
```

The build requires macOS, Xcode 26, CMake ≥ 3.20 and Ninja. It produces a
device slice and a real simulator slice; both run the SH-2 interpreter and the
software renderer, request no executable memory and link no GPU API.

No Sega firmware, BIOS, disc image, or commercial game content is included.

## Licensing

Yaba Sanshiro / Yabause and Pocket Arcade's combined Saturn integration are
distributed under the GNU GPL version 2 or (at your option) any later version;
see `yabause/COPYING.txt` and the per-file headers. libchdr and its bundled
dependencies retain their own licenses (`libchdr/LICENSE.txt`,
`libchdr/deps/*`).

## Synchronization policy

This repository is maintained with the sibling Pocket Arcade app checkout at
`../emu`. Every Saturn bug fix or build change must update both repositories in
the same task. Native source changes are made and verified here (or in
`../emu/Native/SaturnCore` and mirrored); the resulting XCFramework is copied
to `../emu/Native/SaturnCore/SaturnCore.xcframework`. Keep the bridge, CMake
project, Swift adapter, build recipe, pinned revisions, notices, and release
tag aligned.
