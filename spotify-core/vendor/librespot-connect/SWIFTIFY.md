# Swiftify's librespot-connect adapter

Based on librespot-connect 0.8.0 (MIT). The rest of the crate is unchanged.

`Spirc::new_with_local_controls` and `load_local` let Swiftify keep ownership
of its queue and change the player used for a crossfade. Local end-of-track,
preloading and remote skip decisions are left to Swiftify; a remote play or
transfer returns queue ownership to librespot. Spotify autoplay is not resolved
for application-owned entries. Remote contexts retain the upstream behavior.
