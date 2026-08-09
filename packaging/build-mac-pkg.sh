#!/bin/bash
# Packages the Release build into dist/FourColor-<version>-macOS.zip: a real
# .pkg installer for VST3 + AU + Standalone, plus the install README. Same
# layout as the TRIX / DUAL SPACE macOS packages.
set -euo pipefail

VERSION="${1:-1.0.0-rc.1}"
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
ART="$PROJ/build/FourColor_artefacts/Release"
STAGE="$(mktemp -d)"
DIST="$PROJ/dist"
mkdir -p "$DIST"

[ -d "$ART/VST3/FourColor.vst3" ]      || { echo "VST3 not built - run the Release build first"; exit 1; }
[ -d "$ART/AU/FourColor.component" ]   || { echo "AU not built - run the Release build first"; exit 1; }
[ -d "$ART/Standalone/FourColor.app" ] || { echo "Standalone not built - run the Release build first"; exit 1; }

# Stage payloads. COPYFILE_DISABLE stops AppleDouble ._ files landing inside
# the installer payload, where they show up as junk in the BOM.
mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app"
export COPYFILE_DISABLE=1
cp -R "$ART/VST3/FourColor.vst3"      "$STAGE/vst3/"
cp -R "$ART/AU/FourColor.component"   "$STAGE/au/"
cp -R "$ART/Standalone/FourColor.app" "$STAGE/app/"
xattr -cr "$STAGE/vst3/FourColor.vst3" "$STAGE/au/FourColor.component" "$STAGE/app/FourColor.app"
find "$STAGE" -name "._*" -delete

# Ad-hoc signature so the bundles load cleanly on another Mac. This is NOT
# notarisation - the README explains the right-click -> Open step.
codesign --force --deep -s - "$STAGE/vst3/FourColor.vst3"
codesign --force --deep -s - "$STAGE/au/FourColor.component"
codesign --force --deep -s - "$STAGE/app/FourColor.app"

# Signing leaves extended attributes behind; clean again so the BOM holds only
# the real bundles.
xattr -cr "$STAGE/vst3" "$STAGE/au" "$STAGE/app"
find "$STAGE" -name "._*" -delete

pkgbuild --root "$STAGE/vst3" \
         --identifier com.naaman.fourcolor.vst3 --version "$VERSION" \
         --install-location /Library/Audio/Plug-Ins/VST3 \
         "$STAGE/FourColor-VST3.pkg" > /dev/null

pkgbuild --root "$STAGE/au" \
         --identifier com.naaman.fourcolor.au --version "$VERSION" \
         --install-location /Library/Audio/Plug-Ins/Components \
         "$STAGE/FourColor-AU.pkg" > /dev/null

pkgbuild --root "$STAGE/app" \
         --identifier com.naaman.fourcolor.app --version "$VERSION" \
         --install-location /Applications \
         "$STAGE/FourColor-App.pkg" > /dev/null

sed "s/@VERSION@/$VERSION/g" "$PROJ/packaging/Distribution.xml" > "$STAGE/Distribution.xml"

productbuild --distribution "$STAGE/Distribution.xml" \
             --package-path "$STAGE" \
             "$STAGE/FourColor-$VERSION-macOS.pkg" > /dev/null

cp "$PROJ/packaging/README-INSTALL-MAC.txt" "$STAGE/"
( cd "$STAGE" && zip -q -X "FourColor-$VERSION-macOS.zip" \
      "FourColor-$VERSION-macOS.pkg" README-INSTALL-MAC.txt )
mv "$STAGE/FourColor-$VERSION-macOS.zip" "$DIST/"
rm -rf "$STAGE"

echo "wrote $DIST/FourColor-$VERSION-macOS.zip"
