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

#pragma once

#include <QList>
#include <QObject>
#include <QStringList>
#include <QCoroTask>

#include "spotify/SpotifyTypes.h"

/// audio another program plays, as the rust core takes it (swiftify_core.h)
struct SwiftifyLiveBuffer;

namespace swiftify {

/// what the player asks of the audio core: the rust core in the app, a fake in
/// tests. calls that can fail throw SpotifyError with the core's own message;
/// async calls are cancelled (throw Cancelled) when the core is reset under them.
class PlaybackCore : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    /// connects with an oauth access token. returns librespot's own login for
    /// the session, which connectStoredSession reconnects with spending no
    /// token
    virtual QCoro::Task<QString> connectSession(QString accessToken, QString clientId) = 0;
    /// connects with a login a connection returned, and returns the one to
    /// keep from then on
    virtual QCoro::Task<QString> connectStoredSession(QString login, QString clientId) = 0;
    virtual void disconnectSession() = 0;
    /// drops this core for a fresh one, as a logout does. pending calls cancel.
    virtual void reset() = 0;
    /// bumped by reset(), so a call can tell it outlived the core it started on
    virtual quint64 generation() const = 0;

    virtual QCoro::Task<> playTrack(QString spotifyUri) = 0;
    virtual void preloadTrack(const QString &spotifyUri, bool preloadStandby) = 0;
    virtual QCoro::Task<> crossfadeTrack(QString spotifyUri, quint32 durationMs) = 0;
    /// a music file of the person's own, from anywhere on disk
    virtual QCoro::Task<> playFile(QString path) = 0;
    virtual QCoro::Task<> restartAudioOutput() = 0;
    virtual QCoro::Task<QString> lyricsJson(QString spotifyUri) = 0;
    /// a playlist's songs read through the session, shaped like the web api's
    /// items page. the web api only gives the songs of the person's own and
    /// collaborative playlists
    virtual QCoro::Task<QString> playlistJson(QString playlistUri) = 0;
    /// an artist's top songs read through the session, shaped like the web
    /// api's top tracks response, which spotify took away
    virtual QCoro::Task<QString> artistTopTracksJson(QString artistUri) = 0;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void seek(quint32 positionMs) = 0;
    virtual void setVolume(float volume) = 0;
    virtual float volume() const = 0;
    virtual void setStreamQuality(int kbps) = 0;
    virtual void setVolumeNormalization(bool enabled) = 0;
    /// keeps streamed songs in `path`, up to `limitBytes`; both apply the
    /// next time playback connects
    virtual void setAudioCache(const QString &path, qint64 limitBytes) = 0;
    /// applies on the next session; a stable ID keeps reconnects on one device
    virtual void setSpotifyConnect(bool /*enabled*/, const QString & /*deviceId*/) {}
    virtual void setPlaybackShaping(float tempo, float pitchSemitones, bool preservesPitch) = 0;
    virtual void setPreampGain(float gainDb) = 0;
    virtual void setEqualizer(const QList<float> &gainsDb) = 0;

    virtual bool sessionIsValid() const = 0;
    virtual bool audioOutputStalled() const = 0;
    virtual quint32 playbackPositionMs() const = 0;
    virtual quint32 playbackDurationMs() const = 0;
    virtual bool takeEndOfTrack() = 0;

    /// plays what is pushed into `buffer` through the equalizer and the
    /// analyser, in place of whatever played; one already playing carries on.
    /// false if this core can't, or failed to
    virtual bool playLiveBuffer(SwiftifyLiveBuffer *) { return false; }
    virtual void stopLiveBuffer() {}
    /// the rate live audio plays at without being resampled; 0 if unknown
    virtual quint32 liveOutputRate() const { return 0; }
    /// hands the analyser interleaved stereo audio that something else plays
    /// to the speakers, heard at `outputGain` of its full level; nothing of
    /// it is played. may be called from any thread
    virtual void analyseLive(const float * /*samples*/, qsizetype /*count*/, float /*outputGain*/) {}

    /// skips within what something else started on this core (a song picked
    /// in spotify on a phone, playing through a connect device), which goes
    /// on by its own queue. false if nothing else is in charge here
    virtual bool skipRemote(bool /*forward*/) { return false; }

    virtual void setSpectrumEnabled(bool enabled) = 0;
    /// copies up to `capacity` band levels, 0...1; returns how many
    virtual int spectrumLevels(float *out, int capacity) const = 0;

    static constexpr int kSpectrumBands = 48;

signals:
    /// something other than swiftify started `song` here, and what follows
    /// it is up to that, until swiftify plays something again
    void remoteTrackStarted(const swiftify::Song &song);
    /// playback here was paused or resumed from elsewhere
    void remotePlayingChanged(bool playing);
    /// volume was changed from elsewhere (e.g. Spotify Connect or system mixer)
    void remoteVolumeChanged(float volume);
    /// Connect skip buttons while Swiftify owns the queue
    void remoteSkipRequested(bool forward);
};

} // namespace swiftify
