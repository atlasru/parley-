# Parley

Android-first Tox messenger under development in `parley/android-mvp`, based on the HAU history. Kotlin + Jetpack Compose, real c-toxcore JNI, Android Keystore-protected identity/history. No protocol fork or mandatory Parley server.

- [Android build, features and limitations](docs/ANDROID.md)
- [Upstream source and license review](docs/UPSTREAM-REVIEW.md)
- [Verification evidence](docs/VERIFICATION.md)

New Android code and the combined toxcore-linked Android application are GPL-3.0-or-later (`android/COPYING`). Original HAU desktop files remain MIT. This is development software without a complete independent security audit. Tox does not hide all metadata/IP addresses and does not provide guaranteed offline delivery.

The following describes the inherited desktop foundation, not the Android app:

# HAU desktop foundation

HAU is an early desktop messenger built on the decentralized Tox network. It aims to make a private, peer-to-peer identity simple to create and keep on your own device.

**Status:** 0.1.0 foundation release. HAU currently creates one local profile, generates a Tox identity, saves it across restarts, connects to Tox bootstrap nodes, and lets you copy your Tox ID. Contacts and chat are planned for later versions; this release is not yet a usable messenger.

## Platforms

Windows 10/11 x64 is the primary target. Linux x86_64 is supported for development. No account or central HAU server is needed.

## Build

Requirements: CMake 3.24+, C++20 compiler (MSVC 2022 on Windows), Qt 6.5+ with Quick, Quick Controls 2, SQL SQLite driver and Test modules, libsodium, Git, and Ninja. CMake fetches the tagged [c-toxcore v0.2.23](https://github.com/TokTok/c-toxcore/releases/tag/v0.2.23) source; no binary toxcore download is needed.

Linux (Ubuntu): install `libsodium-dev`, `pkg-config`, `libgl1-mesa-dev`, Qt 6 development packages and their QML modules. Then:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/HAU
```

Windows: install Qt 6.8.3 for MSVC 2022, Ninja and vcpkg. `vcpkg.json` installs libsodium for the static triplet:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build
ctest --test-dir build --output-on-failure
```

Set `CMAKE_PREFIX_PATH` if CMake cannot locate Qt. GitHub Actions builds both platforms; the Windows workflow packages Qt plugins and runtime DLLs with `windeployqt` into `HAU-windows-x64.zip`. The Linux tarball is a build artifact, not a standalone AppImage; host Qt and libsodium are required.

## Local data

The app uses Qt's application data directory, under `HAU/profiles/<uuid>/`: `profile.json` stores non-secret metadata, `profile.tox` contains the sensitive Tox identity, and `hau.db` stores schema migrations. Back up `profile.tox` securely. If it is missing or corrupt, HAU refuses to replace it silently. Local data is not encrypted at rest in 0.1.0.

## Roadmap

Next: contacts and one-to-one messaging, followed by protected local storage, file transfer and calls. See [architecture](docs/ARCHITECTURE.md) for the current boundaries.

## License

Original HAU desktop sources: MIT. Toxcore and libsodium retain their upstream licenses. The combined Android APK is GPL-3.0-or-later; see the review linked above.
