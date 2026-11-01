/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

//! Optional Spotify Connect control, sharing Swiftify's players and queue.

use std::{
    collections::VecDeque,
    sync::{Arc, Mutex, Weak, atomic::{AtomicBool, AtomicUsize, Ordering}},
    time::Duration,
};

use librespot::{
    connect::{ConnectConfig, LoadRequest, LoadRequestOptions, LocalControl, Spirc},
    core::{authentication::Credentials, config::DeviceType, session::Session},
    metadata::audio::item::UniqueFields,
    playback::{mixer::Mixer, player::{Player, PlayerEvent}},
};
use serde_json::{Value, json};
use tokio::{sync::{mpsc, oneshot}, task::JoinHandle};

use crate::SpotifyState;

// A reset can replace the core before its old cleanup future has finished.
// Keep one registration alive at a time, including its server-side deletion.
static REGISTRATION: tokio::sync::Mutex<()> = tokio::sync::Mutex::const_new(());

#[derive(Clone, Default)]
pub(super) struct Options {
    pub enabled: bool,
    pub device_id: String,
}

#[derive(Default)]
pub(super) struct Bridge {
    enabled: AtomicBool,
    player: AtomicUsize,
    pub remote_queue: Arc<AtomicBool>,
    events: Mutex<VecDeque<Value>>,
}

impl Bridge {
    fn push(&self, event: Value) {
        if let Ok(mut events) = self.events.lock() {
            // A paused/blocked UI must not accumulate an unlimited event history.
            if events.len() >= 128 {
                events.pop_front();
            }
            events.push_back(event);
        }
    }

    pub fn clear(&self) {
        self.enabled.store(false, Ordering::Release);
        self.remote_queue.store(false, Ordering::Release);
        self.player.store(0, Ordering::Release);
        if let Ok(mut events) = self.events.lock() {
            events.clear();
        }
    }

    pub fn set_player(&self, player: &Arc<Player>) {
        self.player.store(Arc::as_ptr(player) as usize, Ordering::Release);
    }

    pub fn follows_remote_player(&self, player_id: usize) -> bool {
        self.enabled.load(Ordering::Acquire)
            && self.remote_queue.load(Ordering::Acquire)
            && self.player.load(Ordering::Acquire) == player_id
    }

    pub fn take_events(&self) -> String {
        self.events.lock().map(|mut events| {
            serde_json::to_string(&events.drain(..).collect::<Vec<_>>()).unwrap_or_else(|_| "[]".into())
        }).unwrap_or_else(|_| "[]".into())
    }

    pub fn observe(&self, event: &PlayerEvent, state: &Weak<Mutex<SpotifyState>>, local: &AtomicBool) {
        if !self.enabled.load(Ordering::Acquire) {
            return;
        }
        match event {
            PlayerEvent::Loading { .. } if self.remote_queue.load(Ordering::Acquire) => {
                // A phone can transfer playback here while a local file is playing.
                if let Some(state) = state.upgrade() && let Ok(mut state) = state.lock() {
                    // During a crossfade Spirc already controls the incoming slot,
                    // even while Swiftify is waiting for it to finish loading.
                    let target = self.player.load(Ordering::Acquire);
                    if state.standby_player.as_ref().is_some_and(|slot| Arc::as_ptr(&slot.player) as usize == target) {
                        let incoming = state.standby_player.take();
                        let outgoing = state.player.take();
                        state.player = incoming;
                        state.standby_player = outgoing;
                    }
                    if let Some(active) = &state.player {
                        active.active.store(true, Ordering::Release);
                        active.set_gain(1.0);
                    }
                    if let Some(standby) = &state.standby_player {
                        standby.active.store(false, Ordering::Release);
                        standby.set_gain(0.0);
                        standby.player.stop();
                    }
                    if let Some(playback) = state.local_playback.take() {
                        playback.cancel.store(true, Ordering::Release);
                    }
                    if let Some(sink) = &state.local_sink {
                        sink.stop();
                        sink.clear();
                    }
                    if let Some((_, cancel)) = state.live.take() {
                        cancel.store(true, Ordering::Release);
                    }
                    local.store(false, Ordering::Release);
                }
            }
            PlayerEvent::TrackChanged { audio_item } if self.remote_queue.load(Ordering::Acquire) => {
                let (artists, album) = match &audio_item.unique_fields {
                    UniqueFields::Track { album_artists, album, .. } => (album_artists.join(", "), album.clone()),
                    UniqueFields::Episode { show_name, .. } => (show_name.clone(), show_name.clone()),
                    _ => (String::new(), String::new()),
                };
                self.push(json!({"type": "track", "song": {
                    "uri": audio_item.uri, "name": audio_item.name,
                    "artists": artists, "albumName": album,
                    "durationMs": audio_item.duration_ms, "isExplicit": audio_item.is_explicit,
                    "artworkURL": audio_item.covers.first().map(|cover| &cover.url),
                }}));
            }
            PlayerEvent::Playing { .. } if !local.load(Ordering::Acquire) => {
                self.push(json!({"type": "playing", "playing": true}));
            }
            PlayerEvent::Paused { .. } if !local.load(Ordering::Acquire) => {
                // Pausing during a crossfade must silence its outgoing tail too.
                if let Some(state) = state.upgrade() && let Ok(state) = state.lock()
                    && let Some(standby) = &state.standby_player {
                    standby.player.pause();
                }
                self.push(json!({"type": "playing", "playing": false}));
            }
            PlayerEvent::SessionDisconnected { .. } if !local.load(Ordering::Acquire) => {
                if let Some(state) = state.upgrade() && let Ok(state) = state.lock()
                    && let Some(standby) = &state.standby_player {
                    standby.player.stop();
                }
                self.push(json!({"type": "playing", "playing": false}));
            }
            PlayerEvent::Stopped { .. } if self.remote_queue.load(Ordering::Acquire)
                && !local.load(Ordering::Acquire) => {
                self.push(json!({"type": "playing", "playing": false}));
            }
            PlayerEvent::VolumeChanged { volume } => {
                let volume = *volume as f32 / u16::MAX as f32;
                if let Some(state) = state.upgrade() && let Ok(mut state) = state.lock() {
                    state.volume = volume;
                }
                self.push(json!({"type": "volume", "volume": volume}));
            }
            _ => {}
        }
    }
}

pub(super) struct Controller {
    pub spirc: Spirc,
    task: Option<JoinHandle<()>>,
    controls: Option<JoinHandle<()>>,
    registration: Option<tokio::sync::MutexGuard<'static, ()>>,
    runtime: tokio::runtime::Handle,
    bridge: Arc<Bridge>,
}

impl Controller {
    pub async fn start(
        session: Session,
        credentials: Credentials,
        player: Arc<Player>,
        mixer: Arc<dyn Mixer>,
        volume: f32,
        bridge: Arc<Bridge>,
    ) -> Result<Self, String> {
        let registration = REGISTRATION.lock().await;
        let config = ConnectConfig {
            name: "Swiftify".into(),
            device_type: DeviceType::Computer,
            initial_volume: (volume * u16::MAX as f32).round() as u16,
            ..ConnectConfig::default()
        };
        let (tx, mut rx) = mpsc::unbounded_channel();
        bridge.set_player(&player);
        let (spirc, task) = Spirc::new_with_local_controls(
            config, session.clone(), credentials, player, mixer, Some(tx), Arc::clone(&bridge.remote_queue),
        ).await.map_err(|error| format!("Spotify Connect could not start: {error}"))?;
        let (ready_tx, ready_rx) = oneshot::channel();
        let events = Arc::clone(&bridge);
        let controls = tokio::spawn(async move {
            let mut ready = Some(ready_tx);
            while let Some(control) = rx.recv().await {
                match control {
                    LocalControl::Registered => {
                        if let Some(ready) = ready.take() { let _ = ready.send(()); }
                    }
                    LocalControl::Next => events.push(json!({"type": "skip", "forward": true})),
                    LocalControl::Previous => events.push(json!({"type": "skip", "forward": false})),
                    LocalControl::RemotePlayback => {}
                }
            }
        });
        let controller = Self {
            spirc,
            task: Some(tokio::spawn(task)),
            controls: Some(controls),
            registration: Some(registration),
            runtime: tokio::runtime::Handle::current(),
            bridge,
        };
        tokio::time::timeout(Duration::from_secs(25), ready_rx).await
            .map_err(|_| "Spotify Connect registration timed out".to_owned())?
            .map_err(|_| "Spotify Connect stopped before registering the device".to_owned())?;
        controller.bridge.enabled.store(true, Ordering::Release);
        Ok(controller)
    }

    pub fn load(&self, uri: String, player: Arc<Player>) -> Result<(), String> {
        self.bridge.remote_queue.store(false, Ordering::Release);
        self.bridge.set_player(&player);
        self.bridge.take_events();
        self.spirc.load_local(LoadRequest::from_tracks(vec![uri], LoadRequestOptions {
            start_playing: true,
            ..LoadRequestOptions::default()
        }), player).map_err(|error| format!("Spotify Connect could not load the track: {error}"))
    }

    pub fn is_alive(&self) -> bool {
        self.task.as_ref().is_some_and(|task| !task.is_finished())
    }

    pub fn shutdown(self) -> JoinHandle<()> {
        let _ = self.spirc.shutdown();
        self.runtime.clone().spawn(self.stop())
    }

    pub async fn stop(mut self) {
        let _ = self.spirc.shutdown();
        if let Some(mut task) = self.task.take() {
            if tokio::time::timeout(Duration::from_secs(5), &mut task).await.is_err() {
                task.abort();
                let _ = task.await;
            }
        }
        if let Some(controls) = self.controls.take() {
            let _ = controls.await;
        }
    }
}

impl Drop for Controller {
    fn drop(&mut self) {
        let _ = self.spirc.shutdown();
        if let Some(mut task) = self.task.take() {
            let controls = self.controls.take();
            let registration = self.registration.take();
            // FFI destruction runs outside Tokio; use the runtime captured at startup.
            self.runtime.spawn(async move {
                let _registration = registration;
                if tokio::time::timeout(Duration::from_secs(5), &mut task).await.is_err() {
                    task.abort();
                    let _ = task.await;
                }
                if let Some(controls) = controls { let _ = controls.await; }
            });
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn registration_is_disabled_by_default() {
        assert!(!Options::default().enabled);
        let core = crate::SpotifyCore::new();
        assert!(core.set_spotify_connect(true, String::new()).is_err());
        core.set_spotify_connect(true, "stable-device".into()).unwrap();
        assert!(core.state.lock().unwrap().connect_options.enabled);
        core.set_spotify_connect(false, "stable-device".into()).unwrap();
        assert!(!core.state.lock().unwrap().connect_options.enabled);
    }

    #[test]
    fn events_are_bounded_drained_and_cleared() {
        let bridge = Bridge::default();
        for _ in 0..200 { bridge.push(json!({"type": "skip", "forward": true})); }
        let events: Vec<Value> = serde_json::from_str(&bridge.take_events()).unwrap();
        assert_eq!(events.len(), 128);
        assert_eq!(bridge.take_events(), "[]");
        bridge.push(json!({"type": "playing", "playing": false}));
        bridge.remote_queue.store(true, Ordering::Release);
        bridge.clear();
        assert_eq!(bridge.take_events(), "[]");
        assert!(!bridge.remote_queue.load(Ordering::Acquire));
    }

    #[test]
    fn standalone_player_events_are_not_reported_as_connect_controls() {
        let core = crate::SpotifyCore::new();
        core.connect_bridge.observe(&PlayerEvent::VolumeChanged { volume: 0 },
            &Arc::downgrade(&core.state), &core.local_playback_active);
        assert_eq!(core.take_connect_events(), "[]");
        assert_eq!(core.volume(), 0.75);
    }

    #[test]
    fn connect_volume_updates_the_core_and_the_ui() {
        let core = crate::SpotifyCore::new();
        core.connect_bridge.enabled.store(true, Ordering::Release);
        core.connect_bridge.observe(&PlayerEvent::VolumeChanged { volume: u16::MAX },
            &Arc::downgrade(&core.state), &core.local_playback_active);
        assert_eq!(core.volume(), 1.0);
        let events: Vec<Value> = serde_json::from_str(&core.take_connect_events()).unwrap();
        assert_eq!(events[0], json!({"type": "volume", "volume": 1.0}));
    }

    #[test]
    fn remote_end_of_track_does_not_advance_the_application_queue() {
        let core = crate::SpotifyCore::new();
        core.timeline.lock().unwrap().end_of_track = true;
        core.connect_bridge.remote_queue.store(true, Ordering::Release);
        assert!(!core.take_end_of_track());
        core.connect_bridge.remote_queue.store(false, Ordering::Release);
        assert!(core.take_end_of_track());
        assert!(!core.take_end_of_track());
    }
}
