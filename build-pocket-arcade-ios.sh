#!/usr/bin/env bash
# Reproducible build of Pocket Arcade's SaturnCore.xcframework from this
# source root: Yaba Sanshiro 1.20.37 core (SH-2 interpreter, software VDP1/VDP2
# renderer, no OpenGL, no JIT) + libchdr, driven through PocketSaturn/PASaturnBridge.h.
# Produces dist/SaturnCore.xcframework; --headless also builds the macOS harness.
set -euo pipefail

SOURCE_ROOT="$(cd -- "$(dirname -- "$0")" && pwd -P)"
DIST_DIR="${SOURCE_ROOT}/dist"
BUILD_ROOT="${SOURCE_ROOT}/build"
DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET:-17.0}"
JOBS="${BUILD_JOBS:-$(sysctl -n hw.logicalcpu)}"

BUILD_HEADLESS=0
for arg in "$@"; do
  case "${arg}" in
    --headless) BUILD_HEADLESS=1 ;;
    *) echo "unknown argument: ${arg}" >&2; exit 2 ;;
  esac
done

command -v cmake >/dev/null || { echo "cmake is required (brew install cmake)" >&2; exit 1; }
command -v ninja >/dev/null || { echo "ninja is required (brew install ninja)" >&2; exit 1; }
command -v xcodebuild >/dev/null
command -v xcrun >/dev/null

mkdir -p "${DIST_DIR}" "${BUILD_ROOT}"

build_slice() {
  local sdk="$1"          # iphoneos | iphonesimulator
  local build_dir="$2"
  local output_dir="$3"

  cmake -S "${SOURCE_ROOT}/PocketSaturn" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="${sdk}" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DSATURN_UPSTREAM="${SOURCE_ROOT}" \
    -DSATURN_LIBCHDR="${SOURCE_ROOT}/libchdr" \
    -DSATURN_BUILD_FRAMEWORK=ON \
    -DSATURN_BUILD_HEADLESS=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  cmake --build "${build_dir}" --target SaturnCore --parallel "${JOBS}"

  mkdir -p "${output_dir}"
  cp "${build_dir}/SaturnCore" "${output_dir}/SaturnCore"
  strip -S -x "${output_dir}/SaturnCore"
}

build_slice iphoneos "${BUILD_ROOT}/ios-device" "${BUILD_ROOT}/device"
build_slice iphonesimulator "${BUILD_ROOT}/ios-simulator" "${BUILD_ROOT}/simulator"

# Audits ---------------------------------------------------------------------
DEVICE_BINARY="${BUILD_ROOT}/device/SaturnCore"
EXPECTED_EXPORTS="$(grep '^_' "${SOURCE_ROOT}/PocketSaturn/SaturnCore-exported-symbols.txt" | sort)"
ACTUAL_EXPORTS="$(nm -gU "${DEVICE_BINARY}" | awk '{print $NF}' | sort)"
if [[ "${EXPECTED_EXPORTS}" != "${ACTUAL_EXPORTS}" ]]; then
  echo "Symbol isolation audit failed; unexpected exported symbols:" >&2
  diff <(printf '%s\n' "${EXPECTED_EXPORTS}") <(printf '%s\n' "${ACTUAL_EXPORTS}") >&2 || true
  exit 1
fi
HEADER_FLAGS="$(otool -hv "${DEVICE_BINARY}")"
UNDEFINED_SYMBOLS="$(nm -u "${DEVICE_BINARY}")"
LINKED_LIBRARIES="$(otool -L "${DEVICE_BINARY}")"
if grep -q 'WEAK_DEFINES' <<<"${HEADER_FLAGS}"; then
  echo "Symbol isolation audit failed; core exports weak definitions." >&2
  exit 1
fi
if grep -Eq 'pthread_jit_write_protect_np|vm_protect|mach_vm_protect|vm_allocate|mach_vm_remap|sys_icache_invalidate|mprotect' <<<"${UNDEFINED_SYMBOLS}"; then
  echo "JIT API audit failed; the core must not manage executable memory." >&2
  exit 1
fi
if grep -Eq 'OpenGLES|Metal|MoltenVK|libGL' <<<"${LINKED_LIBRARIES}"; then
  echo "Renderer audit failed; the software core must not link a GPU API." >&2
  exit 1
fi

# Framework bundles ---------------------------------------------------------
make_framework() {
  local source_binary="$1"
  local dest_dir="$2"
  local platform="$3"
  local framework="${dest_dir}/SaturnCore.framework"

  rm -rf "${framework}"
  mkdir -p "${framework}/Headers"
  cp "${source_binary}" "${framework}/SaturnCore"
  cp "${SOURCE_ROOT}/PocketSaturn/PASaturnBridge.h" "${framework}/Headers/PASaturnBridge.h"
  cp "${SOURCE_ROOT}/Support/SaturnCore-Info.plist" "${framework}/Info.plist"
  /usr/libexec/PlistBuddy \
    -c "Set :CFBundleSupportedPlatforms:0 ${platform}" \
    -c "Set :MinimumOSVersion ${DEPLOYMENT_TARGET}" \
    "${framework}/Info.plist"
}

make_framework "${BUILD_ROOT}/device/SaturnCore" "${BUILD_ROOT}/device" iPhoneOS
make_framework "${BUILD_ROOT}/simulator/SaturnCore" "${BUILD_ROOT}/simulator" iPhoneSimulator

rm -rf "${DIST_DIR}/SaturnCore.xcframework"
xcodebuild -create-xcframework \
  -framework "${BUILD_ROOT}/device/SaturnCore.framework" \
  -framework "${BUILD_ROOT}/simulator/SaturnCore.framework" \
  -output "${DIST_DIR}/SaturnCore.xcframework"

if [[ "${BUILD_HEADLESS}" == "1" ]]; then
  cmake -S "${SOURCE_ROOT}/PocketSaturn" -B "${BUILD_ROOT}/macos-headless" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DSATURN_UPSTREAM="${SOURCE_ROOT}" \
    -DSATURN_LIBCHDR="${SOURCE_ROOT}/libchdr" \
    -DSATURN_BUILD_FRAMEWORK=OFF \
    -DSATURN_BUILD_HEADLESS=ON \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  cmake --build "${BUILD_ROOT}/macos-headless" --target saturnheadless --parallel "${JOBS}"
  echo "Headless harness: ${BUILD_ROOT}/macos-headless/saturnheadless"
fi

file "${DEVICE_BINARY}"
otool -L "${DEVICE_BINARY}"
echo "Built ${DIST_DIR}/SaturnCore.xcframework from Yaba Sanshiro 1.20.37 (interpreter, software renderer)"
