#!/usr/bin/env bash
# ============================================================================
#  scripts/release.sh — produce the Windows release artifact set (#54/#55).
#
#  Run on the release machine (Windows + MSVC environment, invoked as
#  `sh ./scripts/release.sh`):
#
#    1. clean CMake configure (Release)
#    2. build app + all test suites
#    3. run ctest — a release never ships with failing tests
#    4. compile the Inno Setup installer (ISCC.exe)
#    5. generate SHA-256 checksums for everything in release/
#
#  Environment:
#    OTOHA_RELEASE_VERSION  e.g. 1.0.0   (must match CMake project VERSION)
#    ISCC                   path to ISCC.exe (default: standard install)
# ============================================================================
set -euo pipefail

VERSION="${OTOHA_RELEASE_VERSION:-$(grep -oP 'project\(Otoha VERSION \K[0-9.]+' CMakeLists.txt)}"
BUILD_DIR="build"
RELEASE_DIR="release"

echo "== Otoha release ${VERSION} =="

# 1-2. Clean configure + Release build ----------------------------------------
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release --parallel

# 3. Tests gate ----------------------------------------------------------------
ctest --test-dir "${BUILD_DIR}" -C Release --output-on-failure

# 4. Installer ------------------------------------------------------------------
ISCC_BIN="${ISCC:-/c/Program Files (x86)/Inno Setup 6/ISCC.exe}"
if [ ! -x "${ISCC_BIN}" ] && [ ! -f "${ISCC_BIN}" ]; then
    echo "WARNING: ISCC.exe not found — skipping installer step." >&2
else
    OTOHA_RELEASE_VERSION="${VERSION}" "${ISCC_BIN}" packaging/windows/Otoha.iss
fi

# 5. Checksums (#55) -------------------------------------------------------------
mkdir -p "${RELEASE_DIR}"
cd "${RELEASE_DIR}"

if command -v sha256sum >/dev/null 2>&1; then
    sha256sum Otoha-* > checksums.txt
elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 Otoha-* > checksums.txt
elif command -v certutil >/dev/null 2>&1; then
    : > checksums.txt
    for f in Otoha-*; do
        hash="$(certutil -hashfile "${f}" SHA256 | sed -n 2p | tr -d '[:space:]')"
        echo "${hash}  ${f}" >> checksums.txt
    done
else
    echo "ERROR: no SHA-256 tool available" >&2
    exit 1
fi

echo "== Artifacts =="
ls -la

echo "Release ${VERSION} staged in ${RELEASE_DIR}/"
