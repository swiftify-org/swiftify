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

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <QUrl>
#include <QCoroTask>

class QNetworkAccessManager;
class QTcpSocket;

namespace swiftify {

struct OAuthToken {
    QString accessToken;
    /// spotify may rotate it; empty when it kept the old one
    QString refreshToken;
    QDateTime expiration;
};

/// spotify's authorization-code flow with pkce, as SpotifyOAuth.swift runs it,
/// except that the login page opens in the user's own browser. a one-shot
/// listener on the registered redirect address receives the callback.
class OAuthFlow : public QObject {
    Q_OBJECT

public:
    /// librespot's keymaster client, spotify's own desktop app
    static constexpr auto kPlaybackClientId = "65b708073fc0480ea92a077233ca87bd";
    static constexpr int kPlaybackAuthorizationVersion = 2;
    static constexpr int kLibraryAuthorizationVersion = 3;
    static constexpr quint16 kRedirectPort = 8898;
    static QUrl redirectUri();
    /// where tokens are issued; tests point it at a fake
    static QUrl tokenEndpoint();
    static void setTokenEndpoint(const QUrl &url);

    explicit OAuthFlow(QNetworkAccessManager *network, QObject *parent = nullptr);
    ~OAuthFlow() override;

    /// prepares a login and returns the page to open. listening starts here,
    /// so a port another program holds is reported before the browser opens.
    QUrl beginAuthorization(const QString &clientId, const QStringList &scopes);
    /// trades the callback for tokens. throws SpotifyError.
    QCoro::Task<OAuthToken> exchangeCallback(QUrl callbackUrl, QString clientId);
    QCoro::Task<OAuthToken> refreshAccessToken(QString refreshToken, QString clientId);
    void cancel();
    bool isListening() const { return m_server.isListening(); }

    /// the strict match the swift app applies: http, 127.0.0.1, port 8898 and
    /// the /login path, nothing looser.
    static bool isCallbackUrl(const QUrl &url);

signals:
    /// the browser reached the redirect. emitted once per login.
    void callbackReceived(const QUrl &url);

private:
    void acceptConnection();
    void handleRequest(QTcpSocket *socket);
    void stopListening();
    QCoro::Task<OAuthToken> requestToken(QUrlQuery form);
    static QString randomUrlSafeString(int byteCount);

    QNetworkAccessManager *m_network;
    QTcpServer m_server;
    QString m_codeVerifier;
    QString m_expectedState;
    bool m_receivedCallback = false;
};

} // namespace swiftify
