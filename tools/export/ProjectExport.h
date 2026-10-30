#pragma once

#include <ct/json.hpp>
#include <ct/string.hpp>

namespace k2d
{

// The project's scenes, assets and settings in one pack, its scripts compiled
// to a bytecode bundle, and paths inside the project made relative, as the
// runner reads them when it plays a pack.
// Every string in document that is an absolute path inside projectRoot made
// relative to it, as the editor and the runner resolve them.
void MakePathsRelative(ct::Json& document, const char* projectRoot);
// The same for every scene and prefab file under root, rewritten in place;
// how many changed, or -1 when root cannot be read.
int FixPathsInFiles(const char* root);

bool WriteProjectPack(const char* projectRoot, const char* packPath, ct::String& error);

// A web game in outDir: the web runner from runnerDir (k2d_runner.js and
// k2d_runner.wasm), data.kpak and the page that loads it.
bool ExportWeb(const char* projectRoot, const char* runnerDir, const char* outDir, ct::String& error);

// A desktop game in outDir: the runner at runnerPath copied under the
// project's name (gamePath tells where), and data.kpak beside it.
bool ExportDesktop(const char* projectRoot, const char* runnerPath, const char* outDir, ct::String& gamePath,
                   ct::String& error);

// The Android SDK: ANDROID_HOME, ANDROID_SDK_ROOT, ~/Android/Sdk or
// ~/android/android-sdk, the first that has build-tools; empty when none does.
ct::String FindAndroidSdk();
// The package an Android export of the project gets: org.kinetix2d.<its name>.
ct::String AndroidPackage(const char* projectRoot);

// An Android game in outDir: the runner built for Android (runnerLib, the
// arm64-v8a libk2d_runner.so) as a NativeActivity, data.kpak in its assets, put
// together with the SDK's build tools (aapt2, zipalign, apksigner) and signed with
// the debug key (~/.android/debug.keystore, made with keytool when missing).
bool ExportAndroid(const char* projectRoot, const char* runnerLib, const char* sdkDir, const char* outDir,
                   ct::String& apkPath, ct::String& error);

} // namespace k2d
