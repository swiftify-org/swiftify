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

#include "core/PlaybackCore.h"

#include <QHash>
#include <QSocketNotifier>
#include <QTimer>

#include <memory>

struct SwiftifyCore;

namespace swiftify {

class CoreRequest;

/// spotify-core through its c abi (spotify-core/include/swiftify_core.h).
class RustCore : public PlaybackCore {
    Q_OBJECT

public:
    explicit RustCore(QObject *parent = nullptr);
    ~RustCore() override;

    static QList<float> equalizerFrequencies();

    QCoro::Task<QString> connectSession(QString accessToken, QString clientId) override;
    QCoro::Task<QString> connectStoredSession(QString login, QString clientId) override;
    void disconnectSession() override;
    void reset() override;
    quint64 generation() const override { return m_generation; }

    QCoro::Task<> playTrack(QString spotifyUri) override;
    void preloadTrack(const QString &spotifyUri, bool preloadStandby) override;
    QCoro::Task<> crossfadeTrack(QString spotifyUri, quint32 durationMs) override;
    QCoro::Task<> playFile(QString path) override;
    QCoro::Task<> restartAudioOutput() override;
    QCoro::Task<QString> lyricsJson(QString spotifyUri) override;
    QCoro::Task<QString> playlistJson(QString playlistUri) override;
    QCoro::Task<QString> artistTopTracksJson(QString artistUri) override;

    void play() override;
    void pause() override;
    void seek(quint32 positionMs) override;
    void setVolume(float volume) override;
    float volume() const override;
    void setStreamQuality(int kbps) override;
    void setVolumeNormalization(bool enabled) override;
    void setAudioCache(const QString &path, qint64 limitBytes) override;
    void setSpotifyConnect(bool enabled, const QString &deviceId) override;
    bool skipRemote(bool forward) override;
    void setPlaybackShaping(float tempo, float pitchSemitones, bool preservesPitch) override;
    void setPreampGain(float gainDb) override;
    void setEqualizer(const QList<float> &gainsDb) override;

    bool sessionIsValid() const override;
    bool audioOutputStalled() const override;
    quint32 playbackPositionMs() const override;
    quint32 playbackDurationMs() const override;
    bool takeEndOfTrack() override;

    bool playLiveBuffer(SwiftifyLiveBuffer *buffer) override;
    void stopLiveBuffer() override;
    quint32 liveOutputRate() const override;
    void analyseLive(const float *samples, qsizetype count, float outputGain) override;

    void setSpectrumEnabled(bool enabled) override;
    int spectrumLevels(float *out, int capacity) const override;

private:
    void open();
    void close();
    void drainCompletions();
    void drainConnectEvents();
    /// starts a call through `start` and waits for its completion
    template <typename Start>
    QCoro::Task<QString> request(Start start);

    SwiftifyCore *m_core = nullptr;
    std::unique_ptr<QSocketNotifier> m_notifier;
    QHash<quint64, std::shared_ptr<CoreRequest>> m_pending;
    quint64 m_nextRequestId = 0;
    quint64 m_generation = 0;
    /// given again to each core opened (a reset makes a fresh one)
    int m_streamKbps = 0;
    bool m_normalizeVolume = false;
    QString m_cachePath;
    qint64 m_cacheBytes = 0;
    bool m_connectEnabled = false;
    QString m_connectDeviceId;
    QTimer m_connectEvents;
};

} // namespace swiftify
