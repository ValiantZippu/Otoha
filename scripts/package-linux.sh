#!/usr/bin/env bash
# ============================================================================
#  scripts/package-linux.sh — stage a Linux release tarball (M16 #24/#25).
#
#  Deliberately a TARBALL + .desktop + icon, NOT an AppImage/deb/Flatpak:
#  a plain archive is the lowest-maintenance honest option for v1 (#24:
#  maintain few packaging formats well). Runtime dependencies are the
#  standard desktop stack documented in BUILDING.md; nothing exotic is
#  bundled and no universal-compatibility claims are made (#25).
#
#  Usage:   sh ./scripts/package-linux.sh          (after a Release build)
#  Output:  release/Otoha-<version>-Linux-x64.tar.gz + SHA-256 checksums
# ============================================================================
set -euo pipefail

VERSION="$(grep -oP 'project\(Otoha VERSION \K[0-9.]+' CMakeLists.txt)"
STAGE="release/stage/Otoha-${VERSION}-Linux-x64"
OUT="release"

[ -x build/Otoha ] || [ -x "build/Otoha_artefacts/Release/Otoha" ] || {
    echo "ERROR: build Otoha first (cmake --build build --config Release)" >&2
    exit 1
}
BIN="$(find build -name Otoha -type f -perm -u+x | head -1)"

mkdir -p "${STAGE}"
cp "${BIN}"                                   "${STAGE}/otoha"
cp packaging/linux/io.otoha.Otoha.desktop     "${STAGE}/"
cp packaging/icons/otoha.svg                  "${STAGE}/otoha.svg"
cp packaging/windows/THIRD-PARTY-NOTICES.txt  "${STAGE}/"
cp docs/privacy.md                            "${STAGE}/" 2>/dev/null || true

cat > "${STAGE}/INSTALL.txt" <<EOF
Otoha ${VERSION} — Linux (x64)

Run:      ./otoha
Install:  sudo cp otoha /usr/local/bin/
          sudo cp otoha.svg /usr/local/share/icons/hicolor/scalable/apps/
          sudo cp io.otoha.Otoha.desktop /usr/local/share/applications/

Recordings and projects live in your user data directory; uninstalling
never deletes them.
EOF

cd "${OUT}/stage"
tar -czf "../Otoha-${VERSION}-Linux-x64.tar.gz" "Otoha-${VERSION}-Linux-x64"
cd ../..
sha256sum "release/Otoha-${VERSION}-Linux-x64.tar.gz" > "release/checksums-linux.txt"
rm -rf "${OUT}/stage"

echo "== Staged release/Otoha-${VERSION}-Linux-x64.tar.gz (+ checksums-linux.txt) =="
