# Parley Android MVP

Development branch: `parley/android-mvp`, based on HAU `73ca3c5`. The legacy Qt desktop tree is preserved; Windows feature development waits for Android interoperability verification.

## Build

JDK 21, Android SDK platform 36, NDK 27.2.12479018, CMake 3.22.1, Git, PowerShell 7. Set `ANDROID_HOME` to your SDK. On Linux, install PowerShell to run the same dependency script used by CI.

```powershell
./scripts/native-deps.ps1
cd android
./gradlew.bat :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
./gradlew.bat :app:connectedDebugAndroidTest
```

On Linux use `bash gradlew` instead of `gradlew.bat`. The Gradle 8.14 wrapper verifies its distribution checksum. Native sources are fetched by immutable Git commits, not downloaded opaque binaries. APK: `android/app/build/outputs/apk/debug/app-debug.apk`, arm64-v8a and x86_64. No fake transport or production mock implementation exists. No Google services, push server, phone number or mandatory Parley server.

## Implementation

`NativeBridge` calls c-toxcore directly. Each repository instance has one executor, and every handle operation, callback, mutation, save and `tox_kill` is serialized on it. `tox_iterate` obeys its recommended interval (10–100 ms bounds). Native log callbacks are not installed. App diagnostic events use fixed names and never include IDs, keys, profile bytes, text, passwords or exception stack traces.

The foreground service owns the connection, survives activity recreation, displays an ongoing notification and offers Stop. It uses the Android 14+ `specialUse` type for persistent decentralized messaging, with an explicit manifest subtype; store publication would require policy review. It is started from the visible activity, not a background boot receiver. Android 13+ notification permission is requested by an explicit settings action. Message notifications contain no sender or message preview. No battery exemption is silently requested and no indefinite wake lock is held. Doze/OEM background limits can interrupt delivery; no claim of guaranteed background availability is made.

The Compose UI provides onboarding, contacts/requests, chat, profile/export, a custom bootstrap seed, and system/light/dark themes. IDs and history use stable public keys, not mutable tox friend numbers. UTF-8 byte limits and the full Tox address checksum are checked. Receipts are labeled **received by the client**, not read by a human. Unconfirmed outgoing messages become **unknown** after a process restart; they are not silently retransmitted or marked delivered. Sending to an offline contact is rejected explicitly; there is no durable offline delivery queue.

## Storage and backup

One AES-256-GCM encrypted `AtomicFile` snapshot in `noBackupFilesDir` contains savedata, requests and message history. The key is generated in Android Keystore. Encryption uses a fresh provider-generated IV and authenticates a version context. Corrupt ciphertext/key loss stops loading and preserves the original file; identity is never silently replaced. Android backup is disabled. History is a whole JSON snapshot: appropriate for an initial MVP, not yet a scalable paginated database. In-memory plaintext and managed strings still exist while running. Hardware backing is device-dependent and is not promised.

Profile export requires an 8+ character password and uses native `tox_pass_encrypt`, producing standard encrypted `.tox` savedata, readable by compatible clients such as qTox. Export intentionally excludes local chat history. Import accepts standard plaintext or toxencryptsave-encrypted savedata, validates it with real `tox_new`, and is available only before a local profile exists. Identity backups can be written to a user-chosen document provider; protect that password and do not use the same identity simultaneously in multiple clients. Do not uninstall or clear app data without an identity export: Keystore/device-local history is not a portable backup.

## Security boundaries

Tox encrypts message transport, but it is not an anonymity network: peers and discovery infrastructure can learn IP addresses and connection metadata. Direct connectivity, NAT/firewall conditions and bootstrap node availability affect reachability. Bootstrap seeds are replaceable discovery aids; they do not store or relay offline messages. Both peers generally need to be online. There is no mandatory central infrastructure, protocol fork, independent complete security audit, forward-secrecy claim beyond upstream guarantees, or guarantee against endpoint compromise. Verify IDs out of band. This is a development APK, not an audited secure messenger.

## Verification

Local unit tests cover checksum validation, malformed IDs and UTF-8 message boundaries. Device tests use actual JNI/native sockets for two-peer friend requests, acceptance, bidirectional Unicode messages, receipts, savedata contact restoration, encrypted export/import, wrong password rejection and authenticated history storage/corruption handling. They do not use Mockito or substitute a transport.

`.github/workflows/android.yml` assembles a debug APK, unit tests, lint, and a separate emulator integration job, uploading APKs/reports without publishing a release or merging main. Execution results and external-client evidence belong in `docs/VERIFICATION.md`; a workflow file alone does not demonstrate a passing CI run or qTox GUI interoperability.
