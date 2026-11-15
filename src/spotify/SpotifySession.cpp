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

#include "spotify/SpotifySession.h"

#include "app/ErrorCenter.h"
#include "core/PlaybackCore.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QProcess>
#include <QQmlEngine>
#include <QScopeGuard>
#include <QCoroSignal>

#include <utility>

namespace swiftify {

Q_LOGGING_CATEGORY(lcSession, "swiftify.session")

namespace {
SpotifySession *s_instance = nullptr;
}

const QStringList SpotifySession::kLibraryScopes {
    QStringLiteral("playlist-read-private"),
    QStringLiteral("playlist-read-collaborative"),
    QStringLiteral("playlist-modify-public"),
    QStringLiteral("playlist-modify-private"),
    QStringLiteral("user-read-private"),
    QStringLiteral("user-read-recently-played"),
    QStringLiteral("user-library-read"),
    QStringLiteral("user-library-modify"),
    QStringLiteral("user-read-playback-state"),
    QStringLiteral("user-modify-playback-state"),
    QStringLiteral("ugc-image-upload"),
};

SpotifySession::SpotifySession(PlaybackCore *core, CredentialStore *credentials, QNetworkAccessManager *network,
                               ErrorCenter *errors, QObject *parent)
    : QObject(parent)
    , m_core(core)
    , m_credentials(credentials)
    , m_errors(errors)
    , m_oauth(network)
    , m_webApi(network)
{
    if (!s_instance) {
        s_instance = this;
    }
    connect(&m_oauth, &OAuthFlow::callbackReceived, this, [this](const QUrl &url) { detach(completeLogin(url)); });

    // long enough for a browser to start from cold in the background (half
    // a minute, seen) and spotify to answer, short enough that a login
    // spotify wants something for is shown soon after
    m_repairTimer.setSingleShot(true);
    m_repairTimer.setInterval(45'000);
    connect(&m_repairTimer, &QTimer::timeout, this, &SpotifySession::repairTimedOut);
    m_openInBackground = [](const QUrl &url) {
#ifdef Q_OS_MACOS
        // -g: the browser loads it without coming to the front
        if (QProcess::startDetached(QStringLiteral("/usr/bin/open"),
                                    {QStringLiteral("-g"), url.toString(QUrl::FullyEncoded)})) {
            return;
        }
#endif
        QDesktopServices::openUrl(url);
    };
    if (QCoreApplication *application = QCoreApplication::instance()) {
        connect(application, &QCoreApplication::aboutToQuit, this, &SpotifySession::flushCredentialSaves);
    }
}

SpotifySession::~SpotifySession()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

SpotifySession *SpotifySession::instance()
{
    return s_instance;
}

SpotifySession *SpotifySession::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void SpotifySession::reportError(const QString &message)
{
    m_reportedError = message;
    if (m_errors) {
        m_errors->show(message);
    }
}

void SpotifySession::clearReportedError()
{
    if (m_errors && !m_reportedError.isEmpty() && m_errors->message() == m_reportedError) {
        m_errors->clear();
    }
    m_reportedError.clear();
}

void SpotifySession::setStatus(const QString &status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void SpotifySession::setLibraryStatus(const QString &status)
{
    if (m_libraryStatus != status) {
        m_libraryStatus = status;
        emit libraryStatusChanged();
    }
}

void SpotifySession::setConnected(bool connected)
{
    if (m_connected != connected) {
        m_connected = connected;
        emit connectedChanged();
    }
}

void SpotifySession::setLibraryConnected(bool connected, bool needsUpgrade)
{
    if (m_libraryConnected != connected || m_libraryNeedsUpgrade != needsUpgrade) {
        m_libraryConnected = connected;
        m_libraryNeedsUpgrade = needsUpgrade;
        emit libraryConnectedChanged();
    }
}

void SpotifySession::setEvaluatingStoredLogin(bool evaluating)
{
    if (m_evaluatingStoredLogin != evaluating) {
        m_evaluatingStoredLogin = evaluating;
        emit evaluatingStoredLoginChanged();
    }
}

void SpotifySession::setLibraryClientId(const QString &clientId)
{
    if (m_libraryClientId != clientId) {
        m_libraryClientId = clientId;
        emit libraryClientIdChanged();
    }
}

void SpotifySession::setLoginPurpose(std::optional<LoginPurpose> purpose, const QUrl &url)
{
    m_loginPurpose = purpose;
    m_authorizationUrl = url;
    emit loginChanged();
}

QString SpotifySession::loginClientId(LoginPurpose purpose) const
{
    return purpose == LoginPurpose::Playback ? QString::fromLatin1(OAuthFlow::kPlaybackClientId) : m_libraryClientId;
}

// --- launch ------------------------------------------------------------------------

void SpotifySession::restoreSavedSessions()
{
    detach(restore());
}

QCoro::Task<> SpotifySession::restore()
{
    const quint64 generation = m_generation;
    m_restoring = true;
    const auto finished = qScopeGuard([this] { m_restoring = false; });
    std::optional<StoredCredentials> playback;
    std::optional<StoredCredentials> library;
    try {
        playback = co_await m_credentials->load(CredentialStore::Account::Playback);
        library = co_await m_credentials->load(CredentialStore::Account::Library);
    } catch (const SpotifyError &error) {
        setEvaluatingStoredLogin(false);
        reportError(error.message());
        qCWarning(lcSession) << "reading saved logins:" << error.message();
        co_return;
    }
    if (generation != m_generation) {
        co_return;
    }

    // a playback login from another client id, or an older authorization, is
    // dropped: librespot would refuse it anyway.
    if (playback && (playback->clientId != QLatin1StringView(OAuthFlow::kPlaybackClientId)
                     || playback->authorizationVersion != OAuthFlow::kPlaybackAuthorizationVersion)) {
        playback.reset();
    }

    if (library) {
        setLibraryClientId(library->clientId);
        m_libraryRefreshToken = library->refreshToken;
        m_libraryAuthorizationVersion = library->authorizationVersion;
        setLibraryConnected(m_libraryConnected,
                            library->authorizationVersion != OAuthFlow::kLibraryAuthorizationVersion);
    }
    if (playback) {
        m_refreshToken = playback->refreshToken;
        m_playbackLogin = playback->playbackLogin;
        m_playbackAccessToken = playback->accessToken;
        m_playbackAccessTokenExpiration = playback->accessTokenExpiration;
        setStatus(QStringLiteral("Reconnecting"));
    } else {
        setStatus(QStringLiteral("Sign in to play"));
    }
    if (library) {
        setLibraryStatus(QStringLiteral("Reconnecting"));
    }

    if (!playback && !library) {
        setEvaluatingStoredLogin(false);
        co_return;
    }

    // the library first: the window waits on it, and with an access token
    // still good it takes no time at all. a library being signed in again
    // in the background shows once it is back
    const auto showWhenReady = [this] {
        if (m_repairing != LoginPurpose::Library && !m_repairQueue.contains(LoginPurpose::Library)) {
            setEvaluatingStoredLogin(false);
        }
    };
    if (library) {
        co_await restoreLibrarySession(*library);
    }
    if (generation != m_generation) {
        co_return;
    }
    showWhenReady();
    if (playback) {
        co_await restorePlaybackSession(*playback);
    }
    if (generation != m_generation) {
        co_return;
    }
    showWhenReady();
}

QCoro::Task<> SpotifySession::restorePlaybackSession(StoredCredentials)
{
    const quint64 generation = m_generation;
    try {
        // librespot's own login when there is one: a launch spends no token
        co_await reconnectPlayback();
        throwIfStale(generation);
        setStatus(QStringLiteral("Connected"));
        setConnected(true);
        clearReportedError();
    } catch (const Cancelled &) {
        co_return;
    } catch (const SpotifyError &error) {
        if (generation != m_generation) {
            co_return;
        }
        const bool repairing = m_repairing == LoginPurpose::Playback || m_repairQueue.contains(LoginPurpose::Playback);
        setStatus(repairing ? QStringLiteral("Reconnecting") : QStringLiteral("Disconnected"));
        setConnected(false);
        if (!repairing) {
            reportError(QStringLiteral("Could not restore playback login: %1").arg(error.message()));
        }
        qCWarning(lcSession) << "restoring playback login:" << error.message();
    }
}

QCoro::Task<> SpotifySession::restoreLibrarySession(StoredCredentials credentials)
{
    const quint64 generation = m_generation;
    try {
        // an access token good for a while yet is used as it is: a launch
        // spends no refresh token. one spotify has revoked since answers 401,
        // and the first request refreshes it then
        if (!credentials.accessToken.isEmpty() && credentials.accessTokenExpiration.isValid()
            && credentials.accessTokenExpiration > QDateTime::currentDateTimeUtc().addSecs(300)) {
            m_libraryAccessToken = credentials.accessToken;
            m_libraryAccessTokenExpiration = credentials.accessTokenExpiration;
        } else {
            co_await refreshLibraryAccessToken();
        }
        throwIfStale(generation);
        setLibraryStatus(m_libraryNeedsUpgrade ? QStringLiteral("Reconnect for full access") : QStringLiteral("Connected"));
        setLibraryConnected(true, m_libraryNeedsUpgrade);
        clearReportedError();
        if (m_loadInitialLibrary) {
            co_await m_loadInitialLibrary();
        }
    } catch (const Cancelled &) {
        co_return;
    } catch (const SpotifyError &error) {
        if (generation != m_generation) {
            co_return;
        }
        const bool repairing = m_repairing == LoginPurpose::Library || m_repairQueue.contains(LoginPurpose::Library);
        setLibraryStatus(repairing ? QStringLiteral("Reconnecting") : QStringLiteral("Not connected"));
        setLibraryConnected(false, m_libraryNeedsUpgrade);
        if (!repairing) {
            reportError(QStringLiteral("Could not restore playlist login: %1").arg(error.message()));
        }
        qCWarning(lcSession) << "restoring playlist login:" << error.message();
    }
}

// --- logging in ------------------------------------------------------------------

void SpotifySession::beginPlaybackLogin()
{
    beginLogin(LoginPurpose::Playback, {QStringLiteral("streaming")}, QString::fromLatin1(OAuthFlow::kPlaybackClientId));
}

void SpotifySession::beginLibraryLogin()
{
    setLibraryClientId(m_libraryClientId.trimmed());
    beginLogin(LoginPurpose::Library, kLibraryScopes, m_libraryClientId);
}

void SpotifySession::beginLogin(LoginPurpose purpose, const QStringList &scopes, const QString &clientId)
{
    // the person's own login takes the place of one signing itself in again;
    // the other kind's, if it was that, goes after
    if (m_repairing) {
        if (*m_repairing != purpose) {
            m_repairQueue.prepend(*m_repairing);
        }
        m_repairQueue.removeAll(purpose);
        m_repairing.reset();
        m_automaticLogin = false;
        m_repairTimer.stop();
    }
    try {
        const QUrl url = m_oauth.beginAuthorization(clientId, scopes);
        setLoginPurpose(purpose, url);
        m_errors->clear();
        if (purpose == LoginPurpose::Playback) {
            setStatus(QStringLiteral("Waiting for Spotify login"));
        } else {
            setLibraryStatus(QStringLiteral("Waiting for Spotify login"));
        }
        QDesktopServices::openUrl(url);
    } catch (const SpotifyError &error) {
        setLoginPurpose(std::nullopt);
        reportError(error.message());
        qCWarning(lcSession) << "starting spotify authorization:" << error.message();
    }
}

void SpotifySession::reopenLoginPage()
{
    if (m_authorizationUrl.isValid()) {
        QDesktopServices::openUrl(m_authorizationUrl);
    }
}

QCoro::Task<> SpotifySession::completeLogin(QUrl callbackUrl)
{
    if (!m_loginPurpose) {
        reportError(QStringLiteral("Spotify returned an invalid authorization callback."));
        co_return;
    }
    const LoginPurpose purpose = *m_loginPurpose;
    const QString clientId = loginClientId(purpose);
    const quint64 generation = m_generation;
    m_authorizationUrl = QUrl();
    emit loginChanged();

    bool ok = false;
    try {
        const OAuthToken token = co_await m_oauth.exchangeCallback(callbackUrl, clientId);
        throwIfStale(generation);
        m_errors->clear();
        if (purpose == LoginPurpose::Playback) {
            co_await completePlaybackLogin(token);
        } else {
            co_await completeLibraryLogin(token, clientId);
        }
        ok = true;
    } catch (const Cancelled &) {
        co_return;
    } catch (const SpotifyError &error) {
        if (generation != m_generation) {
            co_return;
        }
        if (purpose == LoginPurpose::Playback) {
            setStatus(QStringLiteral("Disconnected"));
            setConnected(false);
        } else {
            setLibraryStatus(QStringLiteral("Not connected"));
            setLibraryConnected(false, m_libraryNeedsUpgrade);
        }
        reportError(error.message());
        qCWarning(lcSession) << "completing spotify authorization:" << error.message();
    }
    if (generation == m_generation) {
        setLoginPurpose(std::nullopt);
        finishRepair(ok);
    }
}

QCoro::Task<> SpotifySession::completePlaybackLogin(OAuthToken token)
{
    if (token.refreshToken.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::OAuth,
                           QStringLiteral("Spotify did not return a refresh token. Please try connecting again."));
    }
    // kept at once: spotify issued it, and it is the login now. librespot's
    // login for another account, or an older one, goes with what it replaces
    m_refreshToken = token.refreshToken;
    m_playbackAccessToken = token.accessToken;
    m_playbackAccessTokenExpiration = token.expiration;
    m_playbackLogin.clear();
    persist(CredentialStore::Account::Playback);
    adoptPlaybackLogin(co_await connectPlaybackCore(token.accessToken, {}));
    setStatus(QStringLiteral("Connected"));
    setConnected(true);
}

QCoro::Task<> SpotifySession::completeLibraryLogin(OAuthToken token, QString clientId)
{
    if (token.refreshToken.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::OAuth,
                           QStringLiteral("Spotify did not return a refresh token. Please try connecting again."));
    }
    setLibraryClientId(clientId);
    m_libraryAccessToken = token.accessToken;
    m_libraryRefreshToken = token.refreshToken;
    m_libraryAccessTokenExpiration = token.expiration;
    m_libraryAuthorizationVersion = OAuthFlow::kLibraryAuthorizationVersion;
    persist(CredentialStore::Account::Library);
    setLibraryStatus(QStringLiteral("Connected"));
    setLibraryConnected(true, false);
    if (m_loadInitialLibrary) {
        co_await m_loadInitialLibrary();
    }
}

void SpotifySession::cancelLogin()
{
    m_oauth.cancel();
    // and with it whatever was to sign itself in again: the person decides now
    const bool wasRepairing = m_repairing.has_value();
    m_repairing.reset();
    m_repairQueue.clear();
    m_automaticLogin = false;
    m_repairTimer.stop();
    if (wasRepairing && !m_restoring) {
        setEvaluatingStoredLogin(false);
    }
    if (m_loginPurpose == LoginPurpose::Playback) {
        setStatus(m_connected ? QStringLiteral("Connected") : QStringLiteral("Disconnected"));
    } else if (m_loginPurpose == LoginPurpose::Library) {
        setLibraryStatus(m_libraryConnected ? QStringLiteral("Connected") : QStringLiteral("Not connected"));
    }
    setLoginPurpose(std::nullopt);
}

// --- logging out -------------------------------------------------------------------

void SpotifySession::logOut()
{
    detach(logOutAsync());
}

QCoro::Task<> SpotifySession::logOutAsync()
{
    try {
        co_await m_credentials->removeAll();
    } catch (const SpotifyError &error) {
        reportError(error.message());
        qCWarning(lcSession) << "deleting saved spotify logins:" << error.message();
        co_return;
    }

    ++m_generation;
    cancelLogin();
    m_playbackRefresh.reset();
    m_libraryRefresh.reset();
    // a fresh core, as the swift app builds one. anything still waiting on the
    // old one is cancelled.
    m_core->disconnectSession();
    m_core->reset();
    m_errors->clear();

    m_refreshToken.clear();
    m_playbackLogin.clear();
    m_playbackAccessToken.clear();
    m_playbackAccessTokenExpiration = {};
    m_libraryAccessToken.clear();
    m_libraryRefreshToken.clear();
    m_libraryAccessTokenExpiration = {};
    m_libraryAuthorizationVersion = 0;
    m_playbackConnectionIsSuspect = false;
    setLibraryClientId({});

    setStatus(QStringLiteral("Sign in to play"));
    setLibraryStatus(QStringLiteral("Not connected"));
    setConnected(false);
    setLibraryConnected(false, false);
    setEvaluatingStoredLogin(false);
    emit loggedOut();
}

// --- tokens and connections ----------------------------------------------------------

QCoro::Task<QString> SpotifySession::connectPlaybackCore(QString accessToken, QString storedLogin)
{
    // connecting opens no audio output: outputs open when a song starts, so
    // signing in never takes the speakers from whatever else is playing.
    const quint64 coreGeneration = m_core->generation();
    const QString clientId = QString::fromLatin1(OAuthFlow::kPlaybackClientId);
    QString login;
    if (storedLogin.isEmpty()) {
        login = co_await m_core->connectSession(accessToken, clientId);
    } else {
        login = co_await m_core->connectStoredSession(storedLogin, clientId);
    }
    if (m_core->generation() != coreGeneration) {
        throw Cancelled();
    }
    emit playbackConnected();
    co_return login;
}

void SpotifySession::adoptPlaybackLogin(const QString &login)
{
    if (!login.isEmpty() && login != m_playbackLogin) {
        m_playbackLogin = login;
        persist(CredentialStore::Account::Playback);
    }
}

QCoro::Task<> SpotifySession::reconnectPlayback()
{
    const quint64 generation = m_generation;
    if (!m_playbackLogin.isEmpty()) {
        bool connected = false;
        try {
            adoptPlaybackLogin(co_await connectPlaybackCore({}, m_playbackLogin));
            connected = true;
        } catch (const SpotifyError &error) {
            // perhaps only the network, which the token fails on as well;
            // perhaps a login spotify no longer takes, which the token replaces
            qCInfo(lcSession) << "reconnecting with the saved playback login:" << error.message();
        }
        throwIfStale(generation);
        if (connected) {
            co_return;
        }
    }

    // the access token held while it lasts: a reconnect need not spend the
    // refresh token
    const QString held = m_playbackAccessToken;
    QString accessToken = co_await playbackAccessToken(false);
    throwIfStale(generation);
    QString login;
    bool retry = false;
    try {
        login = co_await connectPlaybackCore(accessToken, {});
    } catch (const SpotifyError &) {
        // a token just issued that fails is the network's doing, not the token's
        if (accessToken != held) {
            throw;
        }
        retry = true;
    }
    if (retry) {
        throwIfStale(generation);
        // one the session would not take: a fresh one, once
        accessToken = co_await playbackAccessToken(true);
        throwIfStale(generation);
        login = co_await connectPlaybackCore(accessToken, {});
    }
    throwIfStale(generation);
    adoptPlaybackLogin(login);
}

QCoro::Task<QString> SpotifySession::awaitRefresh(std::shared_ptr<TokenRefresh> refresh)
{
    const auto [ok, accessToken, errorKind, errorMessage] = co_await qCoro(refresh.get(), &TokenRefresh::finished);
    if (!ok) {
        throw SpotifyError(SpotifyError::Kind(errorKind), errorMessage);
    }
    co_return accessToken;
}

QCoro::Task<QString> SpotifySession::playbackAccessToken(bool fresh)
{
    const quint64 generation = m_generation;
    if (m_playbackRefresh) {
        const QString token = co_await awaitRefresh(m_playbackRefresh);
        throwIfStale(generation);
        co_return token;
    }

    if (!fresh && !m_playbackAccessToken.isEmpty() && m_playbackAccessTokenExpiration.isValid()
        && m_playbackAccessTokenExpiration > QDateTime::currentDateTimeUtc().addSecs(60)) {
        co_return m_playbackAccessToken;
    }
    if (m_refreshToken.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::Session, QStringLiteral("Sign in to Spotify in Settings to play."));
    }

    const auto refresh = std::make_shared<TokenRefresh>();
    m_playbackRefresh = refresh;
    const QString clientId = QString::fromLatin1(OAuthFlow::kPlaybackClientId);
    const QString spent = m_refreshToken;
    // each one rotates the login: worth seeing when one is revoked
    qCInfo(lcSession) << "spending the playback refresh token";
    OAuthToken token;
    try {
        token = co_await m_oauth.refreshAccessToken(spent, clientId);
    } catch (const SpotifyError &error) {
        if (m_playbackRefresh == refresh) {
            m_playbackRefresh.reset();
        }
        const SpotifyError shown = generation == m_generation ? repairIfRevoked(error, LoginPurpose::Playback) : error;
        emit refresh->finished(false, {}, int(shown.kind()), shown.message());
        throw shown;
    }
    // held before anything else can run: a refresh after this one has to
    // spend the token spotify just issued, never the one it replaced, or
    // spotify takes the login back. kept in the background after
    if (generation == m_generation) {
        m_refreshToken = token.refreshToken.isEmpty() ? spent : token.refreshToken;
        m_playbackAccessToken = token.accessToken;
        m_playbackAccessTokenExpiration = token.expiration;
        persist(CredentialStore::Account::Playback);
    }
    if (m_playbackRefresh == refresh) {
        m_playbackRefresh.reset();
    }
    emit refresh->finished(true, token.accessToken, 0, {});
    throwIfStale(generation);
    co_return token.accessToken;
}

QCoro::Task<> SpotifySession::ensureFreshPlaybackConnection(bool force)
{
    force = force || m_playbackConnectionIsSuspect;
    // a librespot session can die while its login is still good: a dropped
    // connection, a long sleep, a server-side disconnect. one that lives on
    // needs nothing, however old its token: it logged in with its own
    if (force || !m_connected || !m_core->sessionIsValid()) {
        co_await reconnectPlayback();
        setStatus(QStringLiteral("Connected"));
        setConnected(true);
        clearReportedError();
    }
    m_playbackConnectionIsSuspect = false;
}

QCoro::Task<QString> SpotifySession::refreshLibraryAccessToken()
{
    const quint64 generation = m_generation;
    if (m_libraryRefresh) {
        const QString token = co_await awaitRefresh(m_libraryRefresh);
        throwIfStale(generation);
        co_return token;
    }
    if (m_libraryRefreshToken.isEmpty() || m_libraryClientId.trimmed().isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::Session,
                           QStringLiteral("Your saved Spotify library login cannot be refreshed. Reconnect it once to continue."));
    }

    const auto refresh = std::make_shared<TokenRefresh>();
    m_libraryRefresh = refresh;
    const QString clientId = m_libraryClientId;
    const QString spent = m_libraryRefreshToken;
    qCInfo(lcSession) << "spending the library refresh token";
    OAuthToken token;
    try {
        token = co_await m_oauth.refreshAccessToken(spent, clientId);
    } catch (const SpotifyError &error) {
        if (m_libraryRefresh == refresh) {
            m_libraryRefresh.reset();
        }
        const SpotifyError shown = generation == m_generation ? repairIfRevoked(error, LoginPurpose::Library) : error;
        emit refresh->finished(false, {}, int(shown.kind()), shown.message());
        throw shown;
    }
    // held before anything else can run, as above
    if (generation == m_generation) {
        m_libraryRefreshToken = token.refreshToken.isEmpty() ? spent : token.refreshToken;
        m_libraryAccessToken = token.accessToken;
        m_libraryAccessTokenExpiration = token.expiration;
        persist(CredentialStore::Account::Library);
    }
    if (m_libraryRefresh == refresh) {
        m_libraryRefresh.reset();
    }
    emit refresh->finished(true, token.accessToken, 0, {});
    throwIfStale(generation);
    setLibraryStatus(m_libraryNeedsUpgrade ? QStringLiteral("Reconnect for full access") : QStringLiteral("Connected"));
    setLibraryConnected(true, m_libraryNeedsUpgrade);
    co_return token.accessToken;
}

// --- keeping logins -----------------------------------------------------------------

StoredCredentials SpotifySession::storedCredentials(CredentialStore::Account account) const
{
    if (account == CredentialStore::Account::Playback) {
        return {QString::fromLatin1(OAuthFlow::kPlaybackClientId), m_refreshToken,
                OAuthFlow::kPlaybackAuthorizationVersion, m_playbackLogin, m_playbackAccessToken,
                m_playbackAccessTokenExpiration};
    }
    return {m_libraryClientId, m_libraryRefreshToken, m_libraryAuthorizationVersion, {}, m_libraryAccessToken,
            m_libraryAccessTokenExpiration};
}

void SpotifySession::persist(CredentialStore::Account account)
{
    SaveQueue &queue = m_saves[int(account)];
    queue.next = storedCredentials(account);
    queue.generation = m_generation;
    if (!queue.running) {
        detach(drainSaves(account));
    }
}

QCoro::Task<> SpotifySession::drainSaves(CredentialStore::Account account)
{
    // one write at a time, the latest last: a slow keychain never ends up
    // holding an older token than the one in use
    SaveQueue &queue = m_saves[int(account)];
    queue.running = true;
    while (queue.next) {
        const StoredCredentials credentials = *std::exchange(queue.next, std::nullopt);
        const quint64 generation = queue.generation;
        if (generation != m_generation) {
            continue;
        }
        try {
            co_await m_credentials->save(account, credentials);
        } catch (const SpotifyError &error) {
            qCWarning(lcSession) << "keeping the spotify login:" << error.message();
            reportError(error.message());
        }
        // logged out while it was written: it goes again
        if (generation != m_generation) {
            try {
                co_await m_credentials->removeAll();
            } catch (const SpotifyError &error) {
                qCWarning(lcSession) << "deleting a spotify login kept during a logout:" << error.message();
            }
        }
    }
    queue.running = false;
    if (!m_saves[0].running && !m_saves[1].running) {
        emit credentialsSaved();
    }
}

void SpotifySession::flushCredentialSaves()
{
    // a token spotify is still answering for, or one still being written
    const auto busy = [this] {
        return m_playbackRefresh || m_libraryRefresh || m_saves[0].running || m_saves[1].running;
    };
    if (!busy()) {
        return;
    }
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    QTimer poll;
    connect(&poll, &QTimer::timeout, &loop, [&] {
        if (!busy()) {
            loop.quit();
        }
    });
    deadline.start(3000);
    poll.start(10);
    loop.exec();
}

// --- signing a revoked login in again ----------------------------------------------------

SpotifyError SpotifySession::repairIfRevoked(const SpotifyError &error, LoginPurpose purpose)
{
    if (error.kind() != SpotifyError::Kind::LoginRevoked) {
        return error;
    }
    qCWarning(lcSession) << (purpose == LoginPurpose::Playback ? "playback" : "library")
                         << "login revoked:" << error.message();
    repairLogin(purpose);
    const bool repairing = m_repairing == purpose || m_repairQueue.contains(purpose);
    return {SpotifyError::Kind::LoginRevoked,
            repairing ? QStringLiteral("Spotify signed Swiftify out. Signing in again…")
                      : QStringLiteral("Spotify signed Swiftify out. Reconnect in Settings to continue.")};
}

void SpotifySession::repairLogin(LoginPurpose purpose)
{
    if (m_repairing == purpose || m_repairQueue.contains(purpose)) {
        return;
    }
    // once in a while: a login spotify will not give back is not asked for
    // over and over
    QDateTime &last = m_lastRepair[int(purpose)];
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (last.isValid() && last.secsTo(now) < 600) {
        return;
    }
    // one login at a time; this one follows whatever is under way
    if (m_loginPurpose) {
        m_repairQueue.append(purpose);
        return;
    }
    if (purpose == LoginPurpose::Library && m_libraryClientId.trimmed().isEmpty()) {
        return;
    }

    QUrl url;
    try {
        url = m_oauth.beginAuthorization(loginClientId(purpose), purpose == LoginPurpose::Playback
                                                                     ? QStringList {QStringLiteral("streaming")}
                                                                     : kLibraryScopes);
    } catch (const SpotifyError &error) {
        qCWarning(lcSession) << "signing in again:" << error.message();
        return;
    }
    last = now;
    m_repairing = purpose;
    m_automaticLogin = true;
    setLoginPurpose(purpose, url);
    if (purpose == LoginPurpose::Playback) {
        setStatus(QStringLiteral("Reconnecting"));
    } else {
        setLibraryStatus(QStringLiteral("Reconnecting"));
    }
    qCInfo(lcSession) << "signing" << (purpose == LoginPurpose::Playback ? "playback" : "the library")
                      << "in again in the background";
    m_repairTimer.start();
    m_openInBackground(url);
}

void SpotifySession::finishRepair(bool ok)
{
    const bool wasRepairing = m_repairing.has_value();
    if (wasRepairing && ok) {
        qCInfo(lcSession) << "signed in again";
    }
    m_repairing.reset();
    m_automaticLogin = false;
    m_repairTimer.stop();
    // the other login, if that was revoked too
    if (!m_repairQueue.isEmpty()) {
        repairLogin(m_repairQueue.takeFirst());
    }
    if (wasRepairing && !m_restoring && m_repairing != LoginPurpose::Library
        && !m_repairQueue.contains(LoginPurpose::Library)) {
        setEvaluatingStoredLogin(false);
    }
}

void SpotifySession::repairTimedOut()
{
    if (!m_repairing || !m_automaticLogin) {
        return;
    }
    // spotify wants something of the person, on the page already open in the
    // browser: the login shows as under way, to be finished there
    qCInfo(lcSession) << "spotify did not answer by itself; asking";
    m_automaticLogin = false;
    emit loginChanged();
    if (*m_repairing == LoginPurpose::Playback) {
        setStatus(QStringLiteral("Waiting for Spotify login"));
    } else {
        setLibraryStatus(QStringLiteral("Waiting for Spotify login"));
    }
    if (!m_restoring) {
        setEvaluatingStoredLogin(false);
    }
}

} // namespace swiftify
