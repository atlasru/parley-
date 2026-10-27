# Upstream review — 2026-10-27

Reviewed source snapshots (not just repository descriptions):

| Project | Commit | Findings / reuse |
|---|---|---|
| [atlasru/hau](https://github.com/atlasru/hau/tree/73ca3c5) | `73ca3c5` | MIT. Qt Quick/C++20, one identity, atomic plaintext `.tox` save, four bootstrap seeds, SQLite migration shell. No contacts/messages, import/export, at-rest encryption or Android app. Preserve desktop code and MIT notice. Reuse repository history, concepts and bootstrap seed data. |
| [TokTok/qTox](https://github.com/TokTok/qTox/tree/2271d5ecf66581d25e650b48fd597cff0d146565) | `2271d5ecf66581d25e650b48fd597cff0d146565` | GPL-3.0-or-later on core sources. Current head identifies v1.18.6-rc.1 (2026-10-17). `src/core/core.cpp`: friend callbacks, receipts, TCP relays, normal-message C API. `src/persistence/profile.cpp`: profile persistence. Desktop Qt UI is unsuitable for native Compose reuse. No qTox implementation copied into the Android app. |
| [TokTok/c-toxcore](https://github.com/TokTok/c-toxcore/tree/285b90265d9f79039c3573d00afdfbfff34efd6d) | `285b90265d9f79039c3573d00afdfbfff34efd6d` | GPL-3.0-or-later; source version 0.2.24. The real C API is used through a small JNI bridge: `tox_new`, savedata, `tox_iterate`, friends, messaging, receipts, bootstrap and `toxencryptsave`. No protocol changes. Upstream `third_party/cmp` pinned to `52bfcfa17d2eb4322da2037ad625f5575129cece` (MIT). |
| [evilcorpltd/aTox](https://github.com/evilcorpltd/aTox/tree/e99ba3be576d3611a7569c7b8c9caa3fbb1474a5) | `e99ba3be576d3611a7569c7b8c9caa3fbb1474a5` | GPL-3.0-only. Archived/unmaintained; last reviewed commit adds the notice. Kotlin, XML views, Room, tox4j/Scala API 0.2.3 and native artifacts 0.2.20. Reviewed `ToxService.kt`, Gradle catalog and domain/core layers. Its architecture informed investigation; no application code, resources or tox4j binaries copied. Own JNI avoids old binding/native version coupling and GPL-only source combination. |
| [jedisct1/libsodium](https://github.com/jedisct1/libsodium/tree/77e1ce5d6dee871c49ef211222ba18ef0c486bda) | `77e1ce5d6dee871c49ef211222ba18ef0c486bda` | ISC, version 1.0.22. Real upstream crypto; built from source. Portable implementations with OS randomness; no custom cryptography. CMake source discovery follows upstream Zig build. SIMD implementations are not enabled by guessed CPU flags. |

GitHub branch enumeration for `atlasru/hau` returned only `main`. The account's accessible repository listing also contained an empty `atlasru/parley-`; no additional HAU implementation or branch was found there. This does not establish the absence of inaccessible/private branches elsewhere.

## Licensing decision

Original HAU files remain MIT under root `LICENSE`. New Android/JNI application code is GPL-3.0-or-later; see `android/COPYING`. Linking GPL toxcore means the combined Android APK must be distributed under GPL-compatible terms, with corresponding source and build instructions, upstream notices, and source modifications. This project must not describe the complete APK as MIT or proprietary. qTox and aTox may be studied; copying aTox GPL-3.0-only code would require a fresh compatibility review of the resulting combination.

libsodium ISC and cmp MIT notices are retained in APK assets. Gradle wrapper scripts/JAR are standard Gradle wrapper tooling under Apache-2.0; AndroidX, Kotlin, coroutines and Android build tooling retain their upstream licenses. Run native dependency script at the pinned commits when reproducing the APK; archive those exact sources alongside any future distribution. No release publication is performed by this iteration.
