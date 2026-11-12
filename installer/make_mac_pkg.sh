#!/bin/bash
set -e
A=build/MixPlay_artefacts/Release
R=pkgroot
rm -rf "$R" dist && mkdir -p dist \
  "$R/Library/Audio/Plug-Ins/VST3" "$R/Library/Audio/Plug-Ins/Components" "$R/Applications"
cp -R "$A/VST3/Mix Play.vst3" "$R/Library/Audio/Plug-Ins/VST3/"
cp -R "$A/AU/Mix Play.component" "$R/Library/Audio/Plug-Ins/Components/"
cp -R "$A/Standalone/Mix Play.app" "$R/Applications/"
pkgbuild --root "$R" --identifier com.beatmusic.mixplay --version 0.1.0 dist/MixPlay-macOS.pkg
