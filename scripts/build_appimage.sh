#!/usr/bin/env bash
# =============================================================================
# EASy68K Linux - Standalone AppImage & zsync Delta-Update Builder
# Bundles binaries, Qt6 libraries, platform plugins, AppStream metadata,
# and embeds AppImageUpdate (gh-releases-zsync) update information.
# =============================================================================
set -euo pipefail

BUILD_DIR="${1:-build}"
DIST_DIR="${2:-dist}"
VERSION="${3:-5.16.1}"
GITHUB_USER="${4:-Sebo1411}"
GITHUB_REPO="${5:-EASy68KLinux}"

rm -rf AppDir
mkdir -p \
  AppDir/usr/bin \
  AppDir/usr/lib \
  AppDir/usr/plugins/platforms \
  AppDir/usr/share/applications \
  AppDir/usr/share/metainfo \
  AppDir/usr/share/icons/hicolor/scalable/apps \
  AppDir/usr/share/icons/hicolor/256x256/apps \
  AppDir/usr/share/easy68k \
  AppDir/usr/share/doc/easy68k \
  "${DIST_DIR}"

# 1. Copy compiled binaries
cp "${BUILD_DIR}/easy68k-gui" AppDir/usr/bin/
cp "${BUILD_DIR}/easy68k-asm" AppDir/usr/bin/
cp "${BUILD_DIR}/easy68k-sim" AppDir/usr/bin/
cp "${BUILD_DIR}/easybin" AppDir/usr/bin/

# 2. Bundle Qt6 shared libraries and platform plugins
ldd "${BUILD_DIR}/easy68k-gui" | awk '/=> \/.*libQt6|libicu|libdouble-conversion|libb2|libpcre2-16/ {print $3}' | xargs -r -I{} cp -L "{}" AppDir/usr/lib/
for plugin_dir in /usr/lib/x86_64-linux-gnu/qt6/plugins/platforms /usr/lib64/qt6/plugins/platforms; do
  if [ -d "$plugin_dir" ]; then
    cp -r "$plugin_dir"/* AppDir/usr/plugins/platforms/ || true
  fi
done

# 3. Copy desktop integration, AppStream metainfo, and icons
cp desktop/easy68k.desktop AppDir/
cp desktop/easy68k.desktop AppDir/usr/share/applications/
cp desktop/easy68k.appdata.xml AppDir/usr/share/metainfo/
cp desktop/easy68k.svg AppDir/
cp desktop/easy68k.svg AppDir/usr/share/icons/hicolor/scalable/apps/

# Generate valid 256x256 PNG for .DirIcon if rsvg-convert is available
if command -v rsvg-convert >/dev/null 2>&1; then
  rsvg-convert -w 256 -h 256 desktop/easy68k.svg -o AppDir/easy68k.png
  cp AppDir/easy68k.png AppDir/usr/share/icons/hicolor/256x256/apps/easy68k.png
  cp AppDir/easy68k.png AppDir/.DirIcon
else
  cp desktop/easy68k.svg AppDir/.DirIcon
fi

# 4. Copy Help documentation and 68000 example programs
cp -r docs/help AppDir/usr/share/doc/easy68k/
cp -r examples AppDir/usr/share/easy68k/

# 5. Create AppRun entrypoint
cat << 'EOF' > AppDir/AppRun
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH:-}"
exec "${HERE}/usr/bin/easy68k-gui" "$@"
EOF
chmod +x AppDir/AppRun

# 6. Download appimagetool if not already present
if [ ! -x ./appimagetool ]; then
  wget -q https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage -O appimagetool
  chmod +x appimagetool
fi

APPIMAGE_NAME="easy68k-linux-${VERSION}-x86_64.AppImage"
APPIMAGE_PATH="${DIST_DIR}/${APPIMAGE_NAME}"
UPDATE_INFO="gh-releases-zsync|${GITHUB_USER}|${GITHUB_REPO}|latest|easy68k-linux-*x86_64.AppImage.zsync"

# Build AppImage with embedded zsync update information
ARCH=x86_64 ./appimagetool --appimage-extract-and-run \
  -u "${UPDATE_INFO}" \
  AppDir "${APPIMAGE_PATH}"
chmod +x "${APPIMAGE_PATH}"

# Move or generate .zsync file in DIST_DIR
if [ -f "${APPIMAGE_NAME}.zsync" ] && [ ! -f "${APPIMAGE_PATH}.zsync" ]; then
  mv "${APPIMAGE_NAME}.zsync" "${APPIMAGE_PATH}.zsync"
elif [ ! -f "${APPIMAGE_PATH}.zsync" ] && command -v zsyncmake >/dev/null 2>&1; then
  (cd "${DIST_DIR}" && zsyncmake -u "${APPIMAGE_NAME}" "${APPIMAGE_NAME}")
fi

echo "Built AppImage: ${APPIMAGE_PATH}"
if [ -f "${APPIMAGE_PATH}.zsync" ]; then
  echo "Built AppImage zsync delta metadata: ${APPIMAGE_PATH}.zsync"
fi
