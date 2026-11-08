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

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <functional>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class PlaybackController;
class Settings;

namespace discord {
constexpr quint32 maxFrameSize = 1024 * 1024;
QByteArray frame(quint32 opcode, const QByteArray &payload);
// A complete frame is removed from buffer. Invalid lengths abort the connection.
enum class FrameResult { Incomplete, Complete, Invalid };
FrameResult takeFrame(QByteArray &buffer, quint32 &opcode, QByteArray &payload);
QStringList socketCandidates(const QStringList &roots);
}

/// Local Discord IPC only. No token, SDK, external service, or background thread.
/// Presence is opt-in; pauses keep a labeled track and progress bar.
/// Disabling clears it and closes the connection.
class DiscordPresence : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Discord)
    QML_SINGLETON
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged FINAL)
public:
    static constexpr int kDefaultInactivityTimeoutMs = 10 * 60 * 1000;

    DiscordPresence(PlaybackController *player, Settings *settings, QObject *parent = nullptr,
                    std::function<QStringList()> socketLocator = {},
                    int inactivityTimeoutMs = kDefaultInactivityTimeoutMs);
    ~DiscordPresence() override;
    static DiscordPresence *create(QQmlEngine *, QJSEngine *);
    bool connected() const { return m_ready; }
    QString status() const;
    int inactivityTimeout() const { return m_inactivity.interval(); }
    void setInactivityTimeout(int ms);
    bool inactivityTimerActive() const { return m_inactivity.isActive(); }
    bool idleTerminated() const { return m_idleTerminated; }

signals:
    void connectedChanged();
    void statusChanged();

private:
    void configure();
    void connectNext();
    void disconnected();
    void readFrames();
    void send(quint32 opcode, const QByteArray &payload);
    void scheduleUpdate();
    void update();
    void clear();
    void setReady(bool ready);
    void handlePlaybackStateChanged();
    void onInactivityTimeout();
    enum class Failure { None, Unavailable, AccessDenied, TimedOut, InvalidFrame, RejectedConnection, RejectedActivity };
    void fail(Failure failure, int code = -1);

    PlaybackController *m_player;
    Settings *m_settings;
    std::function<QStringList()> m_socketLocator;
    Failure m_failure = Failure::None;
    int m_failureCode = -1;
    QLocalSocket m_socket;
    QTimer m_retry;
    QTimer m_deadline;
    QTimer m_update;
    QTimer m_inactivity;
    QStringList m_endpoints;
    qsizetype m_endpoint = 0;
    QByteArray m_buffer;
    bool m_ready = false;
    bool m_published = false;
    bool m_idleTerminated = false;
    QJsonObject m_lastMetadata;
    qint64 m_lastPosition = 0;
    qint64 m_lastUpdate = 0;
    quint64 m_localArtworkInstanceId = 0;
    int m_localArtworkIndex = 1;
};

} // namespace swiftify
