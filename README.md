# Parley

Android-first messenger on the standard Tox network. No phone number, mandatory Parley server or protocol fork.

- **Android MVP:** Kotlin, Compose, real toxcore JNI, local profiles, encrypted identity/history, encrypted `.tox` import/export, bootstrap, contacts/requests, bidirectional text, receipts, foreground connection and notifications. Real exchange with Toxic has been verified; see the Android guide and the recorded verification commit on `parley/android-mvp`.
- **Desktop iteration:** Qt Quick/C++ with real toxcore, contacts/requests, conversation UI, text messages and protected local history. Windows identity/history use current-user DPAPI. Portable desktop profile import/export, calls and file transfer remain outstanding. Build/test results must be checked before describing an iteration as verified.

[Android guide](docs/ANDROID.md) · [Windows guide](docs/WINDOWS.md) · [Upstream/license review](docs/UPSTREAM-REVIEW.md)

## Android

JDK 21, Android SDK 36, NDK 27.2.12479018 and CMake 3.22.1:

```powershell
./scripts/native-deps.ps1
cd android
./gradlew.bat :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
```

The APK includes arm64-v8a and x86_64 native libraries compiled from pinned upstream sources. Debug APKs are CI artifacts, not published releases.

## Windows / desktop development

Qt 6.8.3 (minimum 6.5), CMake 3.24+, C++20 compiler, Ninja, libsodium and Git. Windows uses MSVC 2022 and the pinned vcpkg manifest; Linux development needs libsodium-dev, pkg-config, Qt Quick/Controls/SQL/Test and the normal Qt runtime dependencies.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build
ctest --test-dir build --output-on-failure
```

Set `CMAKE_PREFIX_PATH` to your Qt installation if needed. Run `build/bin/Parley.exe`; the inherited CMake target is still named `HAU`. Windows CI packages Qt plugins/runtime with the executable into `Parley-windows-x64.zip`. Linux CI produces a development tarball requiring host Qt/libsodium, not a standalone application image.

Use `Parley.exe --data-dir <empty-test-directory>` for a disposable interoperability profile. Profile directories are locked against simultaneous use. Do not reuse a personal Tox identity in two running clients.

## Local data and delivery

Parley uses its own application-data namespace, separate from original HAU. `profiles/<uuid>/profile.json` is non-secret metadata. Windows stores DPAPI-protected savedata in `profile.identity`; `history.vault` contains protected requests/messages bound to that identity. These files are Windows-user-bound and are not portable `.tox` backups. Desktop portable export/import is not implemented yet. Original HAU profiles are not migrated or overwritten.

Linux is currently a development target: identity remains plaintext `profile.tox`; history uses libsodium secretbox with an owner-only key file. It does not claim Windows DPAPI protection. Full filesystem access can expose the Linux key. The initial history store rewrites an encrypted snapshot and has a 32 MiB payload limit; it is not yet a paginated database. Drafts remain in memory only.

Only a native receipt changes an outgoing message to **received by the client**. This does not mean a person read it. Unconfirmed sent/pending messages become **unknown** after restart and are not silently resent. Sending to an offline contact is rejected without a fake delivery claim. Corrupt protected storage stops the connection and preserves the previous files.

Tox does not hide IP addresses or all metadata. Both peers generally need to be online. Connectivity depends on NAT/firewalls and discovery seeds; there is no guaranteed offline delivery or complete independent Parley/Tox security audit. Device or same-user compromise is outside the protection provided by local encryption.

## Licensing

Original HAU and the new desktop source files retain MIT terms. New Android/JNI source is GPL-3.0-or-later. **The combined toxcore-linked application binaries are GPL-3.0-or-later**, not MIT-only. See root `LICENSE`, `android/COPYING` and the upstream review. Qt, libsodium and other dependencies retain their own licenses. No releases or main merges are performed without user approval.
