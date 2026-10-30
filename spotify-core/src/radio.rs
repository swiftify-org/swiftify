//! a radio station's songs read through the session (Mercury / SpClient).
//!
//! the result is json shaped like `{"tracks": [...]}`, so the front end
//! reads it with json::track.

use http::Method;
use librespot::core::Session;
use serde::{Deserialize, Serialize};

use crate::playlist::{track_metadata, TrackPayload};

#[derive(Deserialize, Default)]
struct RadioStationResponse {
    #[serde(default)]
    tracks: Vec<RadioTrack>,
}

#[derive(Deserialize, Default)]
struct RadioTrack {
    uri: String,
}

#[derive(Serialize)]
struct RadioPayload {
    tracks: Vec<TrackPayload>,
}

pub async fn radio_json(session: &Session, seed_uri: &str) -> Result<String, String> {
    let uris = fetch_radio_track_uris(session, seed_uri).await?;
    if uris.is_empty() {
        return Err("Spotify returned no tracks for this radio station".to_owned());
    }

    let found = track_metadata(session, &uris).await?;
    let tracks: Vec<TrackPayload> = uris
        .iter()
        .filter_map(|uri| found.get(uri).cloned())
        .collect();

    serde_json::to_string(&RadioPayload { tracks })
        .map_err(|error| format!("failed to encode the radio station: {error}"))
}

async fn fetch_radio_track_uris(session: &Session, seed_uri: &str) -> Result<Vec<String>, String> {
    // Collect potential URI variations (e.g. spotify:track:..., spotify:station:track:...)
    let mut candidate_uris = vec![seed_uri.to_owned()];
    if let Some(track_id) = seed_uri.strip_prefix("spotify:track:") {
        candidate_uris.push(format!("spotify:station:track:{track_id}"));
    }

    for candidate in &candidate_uris {
        // 1. Try Mercury protocol: hm://radio-apollo/v3/stations/{candidate}
        let mercury_uri = format!("hm://radio-apollo/v3/stations/{candidate}");
        if let Ok(future) = session.mercury().get(mercury_uri) {
            if let Ok(response) = future.await {
                if let Some(payload) = response.payload.first() {
                    if let Ok(station) = serde_json::from_slice::<RadioStationResponse>(payload) {
                        let uris: Vec<String> = station
                            .tracks
                            .into_iter()
                            .map(|t| t.uri)
                            .filter(|u| u.starts_with("spotify:track:"))
                            .collect();
                        if !uris.is_empty() {
                            return Ok(uris);
                        }
                    }
                }
            }
        }

        // 2. Try SpClient endpoint: /radio-apollo/v3/stations/{candidate}
        let endpoint = format!("/radio-apollo/v3/stations/{candidate}");
        if let Ok(bytes) = session.spclient().request(&Method::GET, &endpoint, None, None).await {
            if let Ok(station) = serde_json::from_slice::<RadioStationResponse>(&bytes) {
                let uris: Vec<String> = station
                    .tracks
                    .into_iter()
                    .map(|t| t.uri)
                    .filter(|u| u.starts_with("spotify:track:"))
                    .collect();
                if !uris.is_empty() {
                    return Ok(uris);
                }
            }
        }

        // 3. Try with URL-encoded colons: /radio-apollo/v3/stations/{encoded}
        let encoded = candidate.replace(':', "%3A");
        let endpoint_enc = format!("/radio-apollo/v3/stations/{encoded}");
        if let Ok(bytes) = session.spclient().request(&Method::GET, &endpoint_enc, None, None).await {
            if let Ok(station) = serde_json::from_slice::<RadioStationResponse>(&bytes) {
                let uris: Vec<String> = station
                    .tracks
                    .into_iter()
                    .map(|t| t.uri)
                    .filter(|u| u.starts_with("spotify:track:"))
                    .collect();
                if !uris.is_empty() {
                    return Ok(uris);
                }
            }
        }
    }

    Err(format!("Spotify would not send a radio station for {seed_uri}"))
}
