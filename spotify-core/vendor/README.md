# Patched librespot core

`librespot-core` 0.8.0 is vendored so Swiftify can work around
[librespot issue #1477](https://github.com/librespot-org/librespot/issues/1477).

Spotify currently rejects the native iOS AP handshake with `TryAnotherAP`. Spoofing only that
handshake lets the session connect but can leave subsequent authentication, HTTP, metadata, and
audio-key requests with a conflicting iOS identity, causing every track to fail loading.

The source change in `config.rs` gives iOS builds a consistent Linux protocol identity end-to-end.
This matches the working desktop/Keymaster OAuth path and does not change the iOS compilation
target or Core Audio output.

Remove the crates.io patch in `spotify-core/Cargo.toml` when upstream resolves the iOS issue.

## Connect queue adapter

`librespot-connect` 0.8.0 is also vendored for optional Spotify Connect registration.
Its small application-queue adapter preserves Swiftify's queue and two-player
crossfades while using upstream registration, state reporting and remote contexts.
See `librespot-connect/SWIFTIFY.md` for the added interface.
