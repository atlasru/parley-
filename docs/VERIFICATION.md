# Verification record

## Iteration 1 — Android build (2026-10-27)

Local command: `gradlew.bat :app:assembleDebug :app:testDebugUnitTest :app:lintDebug :app:assembleDebugAndroidTest --no-daemon`.

- **PASS**: Android debug APK, arm64-v8a + x86_64, native toxcore/libsodium/JNI built from pinned sources.
- **PASS**: 3 JVM unit tests, 0 failures/errors (Tox address checksum/malformed inputs, UTF-8 limits).
- **PASS**: Android lint, 0 errors, 7 warnings (dependency updates and explicit modern backup rules).
- **PASS**: instrumentation APK compiled.
- Native device execution, external-client exchange and remote CI: **pending**, not established by compilation.

The first build attempt lost its Gradle daemon on this 7.5 GiB host while a 2 GiB emulator and a large unused icon dependency were active. The final build uses a 1536 MiB Gradle heap, two workers, in-process Kotlin compilation, and the small icon set. It succeeded locally in 4m24s. No transport mock was introduced to get the build passing.

Raw local build logs are ignored under `.tools/`; portable test evidence and CI links will be recorded as execution completes. This record deliberately distinguishes compiled features from verified runtime behavior.
