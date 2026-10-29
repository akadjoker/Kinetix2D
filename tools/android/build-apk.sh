#!/usr/bin/env bash
# A game as an Android APK: the runner built with the NDK as a NativeActivity
# library, and the game's data.kpak (from Export Web or Export Desktop) as an asset.
#
#   tools/android/build-apk.sh data.kpak [name] [package] [run]
#
# name defaults to the project folder of <project>/export/<target>/data.kpak,
# package to org.kinetix2d.<name>;
# "run" installs the APK on the connected device and starts it.
# ANDROID_HOME and ANDROID_NDK_HOME default to ~/android/android-sdk and
# ~/android/android-ndk-r30.
set -euo pipefail

PACK=$(realpath "${1:?usage: build-apk.sh data.kpak [name] [package] [run]}")
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SDK=${ANDROID_HOME:-$HOME/android/android-sdk}
NDK=${ANDROID_NDK_HOME:-$HOME/android/android-ndk-r30}
BUILD_TOOLS=$SDK/build-tools/35.0.1
PLATFORM=$SDK/platforms/android-35/android.jar
OUT=${K2D_ANDROID_OUT:-$ROOT/build-android}
STAGE=$OUT/apk
KEYSTORE=$HOME/.android/debug.keystore
ABI=arm64-v8a

NAME=${2:-}
if [ -z "$NAME" ] && [ "$(basename "$(dirname "$(dirname "$PACK")")")" = export ]; then
    NAME=$(basename "$(dirname "$(dirname "$(dirname "$PACK")")")")
fi
NAME=${NAME:-Game}
ID=$(echo "$NAME" | tr '[:upper:]' '[:lower:]' | tr -c 'a-z0-9\n' '_' | sed 's/^[0-9_]*//')
PACKAGE=${3:-org.kinetix2d.${ID:-game}}
APK=$OUT/$ID.apk

cmake -S "$ROOT" -B "$OUT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release -DK2D_RUNTIME_OUTPUT_DIRECTORY="$OUT/bin" > /dev/null
ninja -C "$OUT" k2d_runner

rm -rf "$STAGE"
mkdir -p "$STAGE/lib/$ABI" "$STAGE/assets"
"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" -o "$STAGE/lib/$ABI/libk2d_runner.so" "$OUT/runner/libk2d_runner.so"
cp "$PACK" "$STAGE/assets/data.kpak"

cat > "$STAGE/AndroidManifest.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="$PACKAGE" android:versionCode="1" android:versionName="1.0">
    <uses-feature android:glEsVersion="0x00030000" android:required="true" />
    <uses-feature android:name="android.hardware.touchscreen" android:required="false" />
    <uses-feature android:name="android.hardware.gamepad" android:required="false" />
    <application android:label="$NAME" android:hasCode="false" android:extractNativeLibs="true"
        android:theme="@android:style/Theme.NoTitleBar.Fullscreen">
        <activity android:name="android.app.NativeActivity" android:exported="true"
            android:screenOrientation="sensorLandscape" android:launchMode="singleTask"
            android:configChanges="orientation|screenSize|screenLayout|smallestScreenSize|keyboard|keyboardHidden|navigation|uiMode">
            <meta-data android:name="android.app.lib_name" android:value="k2d_runner" />
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
XML

# The pack is compressed already: stored as it is, so the asset is read without inflating.
"$BUILD_TOOLS/aapt2" link -o "$OUT/unsigned.apk" --manifest "$STAGE/AndroidManifest.xml" \
    -I "$PLATFORM" -A "$STAGE/assets" -0 kpak --min-sdk-version 26 --target-sdk-version 35
(cd "$STAGE" && zip -q -0 -r "$OUT/unsigned.apk" lib)
"$BUILD_TOOLS/zipalign" -p -f 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
if [ ! -f "$KEYSTORE" ]; then
    mkdir -p "$(dirname "$KEYSTORE")"
    keytool -genkeypair -keystore "$KEYSTORE" -storepass android -keypass android \
        -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US" > /dev/null 2>&1
fi
"$BUILD_TOOLS/apksigner" sign --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android \
    --out "$APK" "$OUT/aligned.apk"
rm -f "$OUT/unsigned.apk" "$OUT/aligned.apk"
echo "$APK"

if [ "${4:-}" = "run" ]; then
    ADB=$SDK/platform-tools/adb
    "$ADB" install -r "$APK"
    "$ADB" shell am start -n "$PACKAGE/android.app.NativeActivity"
fi
