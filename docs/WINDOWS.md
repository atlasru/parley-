# Parley desktop iteration

The desktop client uses the same standard Tox friend/message APIs as compatible clients. All toxcore access is serialized on a dedicated worker. The Qt interface receives immutable snapshots via queued signals; remote text is rendered as plain text. No mock transport is present in production.

## Implemented in the messaging iteration

- Add a full Tox ID, receive/accept/reject friend requests, display contact presence.
- Send/receive UTF-8 text, native delivery receipts, and durable request/message history.
- Light/dark themes, per-conversation in-memory drafts, explicit send failures and unknown status after restart.
- Current-user DPAPI for both Windows identity and history, atomic writes, owner binding and integrity failures that preserve the original file.
- A single-writer profile lock. `--data-dir` supports isolated manual interoperability profiles.

The executable is now `Parley.exe`, though the inherited CMake target remains `HAU`. This is still an early client: portable desktop import/export, calls, file transfer, Windows notifications/tray behavior and history pagination are not implemented. Android retains its existing foreground-service/notification and portable profile functionality.

## Storage and boundaries

Windows `profile.identity` and `history.vault` are DPAPI-wrapped and tied to the Windows user credential context. Plaintext fallback and automatic identity replacement are disabled. History carries the identity public key inside the protected payload; another identity cannot silently adopt that history. DPAPI output buffers are zeroed before release. Non-secret profile metadata remains JSON. DPAPI does not protect against malicious code executing as the same user, OS compromise or every recovery/credential scenario. The encrypted files are not portable backups.

Linux development stores plaintext identity and libsodium-secretbox history with a private filesystem key, not DPAPI. Native test fixtures use real sockets and this actual persistence path on each platform. The history snapshot has a 32 MiB limit; reaching it stops the connection without deleting older messages.

Profile identity and chat history are separate atomic files. A failure stops the session. Request acceptance is reconciled against native contacts on restart, and unconfirmed outgoing statuses become unknown. There is no claim of an atomic transaction spanning network delivery and local storage, nor automatic retry of uncertain sends.

## Verification

The desktop Qt test suite retains the 100 identity-restart stress test and Windows DPAPI checks. New tests exercise authenticated history round trips, wrong-owner/ciphertext rejection, profile locking, and two production ToxService instances over real local UDP sockets: request, acceptance, Unicode messages in both directions, receipts and history/contact restoration after restart. Public discovery can be disabled for deterministic tests; the transport remains actual toxcore.

Inspect GitHub Actions for the exact commit's outcome. A source commit alone is not evidence that Windows, the GUI or an external-client scenario passed. Desktop toxcore/libsodium dependency pins still follow the inherited desktop build and should be aligned with Android before a Windows release.
