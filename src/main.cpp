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

#include "app/AppController.h"
#include "app/ErrorCenter.h"
#include "app/Fonts.h"
#include "app/Logging.h"
#include "app/Paths.h"
#include "app/Settings.h"
#include "system/DiscordPresence.h"
#include "app/Translations.h"
#include "artwork/ArtworkCache.h"
#include "audio/AudioShaping.h"
#include "core/RustCore.h"
#include "library/LibraryStore.h"
#include "library/LocalLibrary.h"
#include "motion/ScrollMomentum.h"
#include "playback/PlaybackController.h"
#include "render/SpectrumSource.h"
#include "stats/PlayHistory.h"
#include "stats/StatsStore.h"
#include "spotify/CredentialStore.h"
#include "spotify/SpotifySession.h"
#include "system/SingleInstance.h"
#include "theme/PlayerStyle.h"
#include "theme/Theme.h"

#ifdef SWIFTIFY_HAS_SOLOIST
#include "soloist/SoloistCore.h"
#endif
#if defined(SWIFTIFY_HAS_DBUS) && defined(Q_OS_LINUX)
#include "system/dbus/Mpris.h"
#include "system/dbus/SleepMonitor.h"
#endif
#if defined(SWIFTIFY_DESKTOP_INTEGRATION) && defined(SWIFTIFY_HAS_DBUS) && defined(Q_OS_LINUX)
#include "system/linux/AudioDeviceMonitor.h"
#endif
#ifdef Q_OS_MACOS
#include "system/macos/MacWindow.h"
#include "system/macos/NowPlaying.h"
#endif
#ifdef Q_OS_WIN
#include "system/windows/WindowsIntegration.h"
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
#include <QApplication>
#else
#include <QGuiApplication>
#endif
#include <QEvent>
#include <QIcon>
#include <QPlatformSurfaceEvent>
#include <QSet>
#include <QWindow>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSocketNotifier>
#include <QUuid>
#include <QtQml/qqmlextensionplugin.h>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifdef Q_OS_MACOS
#include <mach-o/dyld.h>
#include <unistd.h>

#include <climits>
#include <cstdint>
#include <cstdlib>
#endif

Q_IMPORT_QML_PLUGIN(SwiftifyPlugin)

using namespace swiftify;

namespace {

#ifdef Q_OS_UNIX
int s_quitSignals[2] = {-1, -1};

void forwardQuitSignal(int)
{
    const char byte = 1;
    [[maybe_unused]] const ssize_t written = ::write(s_quitSignals[1], &byte, 1);
}

/// a terminate (a kill, the session ending) quits as the menu does, rather
/// than stopping dead: what is still being kept gets written first, a spotify
/// login just rotated above all. a second one stops it at once
void quitGracefullyOnSignals(QCoreApplication &application)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_quitSignals) != 0) {
        return;
    }
    auto *notifier = new QSocketNotifier(s_quitSignals[0], QSocketNotifier::Read, &application);
    QObject::connect(notifier, &QSocketNotifier::activated, &application, [notifier] {
        notifier->setEnabled(false);
        char byte = 0;
        [[maybe_unused]] const ssize_t read = ::read(s_quitSignals[0], &byte, 1);
        for (const int signal : {SIGTERM, SIGHUP, SIGINT}) {
            std::signal(signal, SIG_DFL);
        }
        QCoreApplication::quit();
    });
    struct sigaction action {};
    action.sa_handler = forwardQuitSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    for (const int signal : {SIGTERM, SIGHUP, SIGINT}) {
        sigaction(signal, &action, nullptr);
    }
}
#endif

/// how the scene graph keeps what it draws, set before anything is drawn.
/// a variable already in the environment wins, for measuring
void tuneSceneGraph()
{
    const auto fallback = [](const char *name, const char *value) {
        if (!qEnvironmentVariableIsSet(name)) {
            qputenv(name, value);
        }
    };
    // the shared texture small images are packed into is sized from the
    // window, rounded up to a power of two: 4096 by 2048, 32 MB of video
    // memory, for a retina window the main one's size. the only images the
    // app shows in any number are covers, drawn through a shader that cannot
    // read from it, so qt copied each small one back out into a texture of
    // its own and kept the shared one (4 MB at 1024 square) besides: nothing
    // goes in it, and it is kept small should anything ever do
    fallback("QSG_ATLAS_WIDTH", "64");
    fallback("QSG_ATLAS_HEIGHT", "64");
    fallback("QSG_ATLAS_SIZE_LIMIT", "1");
    // a decoded image is let go of once it is on the gpu, rather than kept
    // in memory beside its texture for as long as it shows: every cover on
    // screen was held twice. a window that draws an image another window
    // already drew loads its own (ArtworkImage.cache)
    fallback("QSG_TRANSIENT_IMAGES", "1");
    // icons are drawn into their atlas by a painter (MaterialSymbol), which
    // draws a glyph over 64 pixels by filling its outline rather than by the
    // platform's rasteriser, as qt's own text does at any size: a big
    // outlined icon came out a tenth lighter than it did as text
    fallback("QT_MAX_CACHED_GLYPH_SIZE", "512");
}

#ifdef Q_OS_MACOS
/// macos's allocator keeps what a process frees for its next allocations,
/// spread over fronts for each thread and cluster of cores: a third of the
/// heap's pages held nothing live, and counted against the app all the same.
/// asked as the process starts to favour space (MallocSpaceEfficient), it
/// holds a fraction of that, and the window draws no slower. it reads the
/// variable only then: the bundle sets it for a launch from the finder or
/// the dock (LSEnvironment), and a launch without it starts over with it
void startCompact(char *argv[])
{
    if (std::getenv("MallocSpaceEfficient") != nullptr) {
        return;
    }
    setenv("MallocSpaceEfficient", "1", 1);
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        execv(path, argv);
    }
    // it could not start over; it carries on as it is
}

/// every window's metal layer keeps two drawables, and its view reports a
/// colour space that keeps its profile at hand (MacWindow.h). set as its
/// surface is made, and again when it is first shown, as the layer and the
/// frame may come only then
class WindowTuner : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        const bool created = event->type() == QEvent::PlatformSurface
            && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated;
        if (created || event->type() == QEvent::Expose) {
            if (auto *window = qobject_cast<QWindow *>(object)) {
                tune(window, m_drawables, macos::limitDrawables);
                tune(window, m_colorSpaces, macos::holdColorSpace);
            }
        }
        return false;
    }

private:
    void tune(QWindow *window, QSet<QWindow *> &done, bool (*step)(QWindow *))
    {
        if (done.contains(window) || !step(window)) {
            return;
        }
        done.insert(window);
        connect(window, &QObject::destroyed, this, [&done, window] { done.remove(window); });
    }

    QSet<QWindow *> m_drawables;
    QSet<QWindow *> m_colorSpaces;
};
#endif

#ifdef Q_OS_LINUX
/// alsa audio streams routing through pipewire or pulseaudio default to generic
/// labels like "PipeWire ALSA [swiftify]" or "PulseAudio ALSA [swiftify]".
/// these variables tell their alsa plugins to present the stream as Swiftify
/// with its application icon and music media role in pavucontrol and mixers
void setupAudioEnvironment()
{
    if (!qEnvironmentVariableIsSet("PIPEWIRE_ALSA")) {
        qputenv("PIPEWIRE_ALSA",
                "{ application.name=\"Swiftify\" application.icon-name=\"swiftify\" node.name=\"Swiftify\" "
                "node.description=\"Swiftify\" media.name=\"Playback\" media.role=\"music\" }");
    }
    if (!qEnvironmentVariableIsSet("PULSE_PROP")) {
        qputenv("PULSE_PROP", "application.name=Swiftify application.icon_name=swiftify media.role=music");
    }
}
#endif


} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_MACOS
    startCompact(argv);
#endif
#ifdef Q_OS_WIN
    WindowsIntegration::initializeProcess();
#endif
    logging::setupLogging();
#ifdef Q_OS_LINUX
    setupAudioEnvironment();
#endif
    tuneSceneGraph();
    QGuiApplication::setApplicationName(QStringLiteral("Swiftify"));
    QGuiApplication::setApplicationVersion(QStringLiteral(SWIFTIFY_VERSION));
    // the wayland app id, which hyprland window rules and the .desktop file match.
    QGuiApplication::setDesktopFileName(QStringLiteral("swiftify"));
    // the tray menu can fall back to widgets on linux
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
    QApplication application(argc, argv);
#else
    QGuiApplication application(argc, argv);
#endif
#ifdef Q_OS_WIN
    application.setQuitOnLastWindowClosed(false);
#endif
#ifdef Q_OS_UNIX
    quitGracefullyOnSignals(application);
#endif
#ifndef Q_OS_MACOS
    // x11 shows this; wayland goes by the desktop entry, macos by the bundle
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Swiftify/resources/swiftify-icon.png")));
#endif

    SingleInstance instance;
    if (instance.forwardToRunningInstance(QCoreApplication::arguments())) {
        return EXIT_SUCCESS;
    }
#ifdef Q_OS_MACOS
    WindowTuner windowTuner;
    application.installEventFilter(&windowTuner);
#endif
#ifdef Q_OS_LINUX
    // a trackpad swipe glides on after the fingers lift, as macos does itself
    ScrollMomentum scrollMomentum;
    application.installEventFilter(&scrollMomentum);
#endif

    // the plain style: every control swiftify shows is drawn by its own components.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    fonts::loadIconFont();
    fonts::loadInterfaceFont();

    ErrorCenter errors(nullptr);
    Settings settings(nullptr);
    Translations translations(&settings);
    Theme theme(&settings);
    QNetworkAccessManager network;
    RustCore rustCore;
    QString connectDeviceId = settings.value(QStringLiteral("spotifyConnectDeviceId")).toString();
    if (connectDeviceId.isEmpty()) {
        connectDeviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("spotifyConnectDeviceId"), connectDeviceId);
    }
#ifdef SWIFTIFY_HAS_SOLOIST
    // spotify's songs through soloist when it is installed and has a key, with
    // the rust core keeping the login, lyrics and local files
    std::unique_ptr<SoloistCore> soloistCore;
    if (auto soloist = SoloistCore::detect()) {
        soloistCore = std::make_unique<SoloistCore>(&rustCore, std::move(*soloist));
    }
    PlaybackCore &core = soloistCore ? static_cast<PlaybackCore &>(*soloistCore) : rustCore;
#else
    PlaybackCore &core = rustCore;
#endif
    const auto applyConnect = [&] {
        // Soloist is already a Connect client; never register its fallback too.
#ifdef SWIFTIFY_HAS_SOLOIST
        rustCore.setSpotifyConnect(settings.spotifyConnectEnabled() && !soloistCore, connectDeviceId);
#else
        rustCore.setSpotifyConnect(settings.spotifyConnectEnabled(), connectDeviceId);
#endif
    };
    applyConnect();
    QObject::connect(&settings, &Settings::spotifyConnectEnabledChanged, &rustCore, applyConnect);
    // what playback connects with: the streaming quality, and how much of
    // what it streams is kept so a song played again needs no download
    const auto applyStreaming = [&] {
        core.setStreamQuality(settings.streamingQuality());
        core.setVolumeNormalization(settings.normalizeVolume());
        core.setAudioCache(paths::cacheDirectory() + QStringLiteral("/songs"),
                           qint64(settings.songCacheMegabytes()) * 1000 * 1000);
    };
    applyStreaming();
    QObject::connect(&settings, &Settings::streamingQualityChanged, &core, applyStreaming);
    QObject::connect(&settings, &Settings::normalizeVolumeChanged, &core, applyStreaming);
    QObject::connect(&settings, &Settings::songCacheMegabytesChanged, &core, applyStreaming);
    KeychainCredentialStore credentials;
    SpotifySession session(&core, &credentials, &network, &errors);
    PlaybackController player(&core, &session, &settings, &errors);
    DiscordPresence discord(&player, &settings);
    LibraryStore library(&session, &settings, &errors);
    library.attachPlayer(&player);
    AudioShaping sound(&core, &settings, &session);
    LocalLibrary localFiles(&settings);
    library.attachLocalFiles(&localFiles);
    // a song whose tags were edited shows as it is now wherever it is queued
    QObject::connect(&localFiles, &LocalLibrary::scanned, &player,
                     [&] { player.refreshSongDetails(localFiles.allSongs()); });
    AppController app(nullptr);
    ArtworkCache artwork(&network);
    PlayerStyle playerStyle(&player, &theme, &artwork);
    SpectrumSource spectrum(&core, &player);
    PlayHistory listeningHistory(PlayHistory::defaultPath());
    PlayTracker listeningTracker(&player, &listeningHistory);
    StatsStore stats(&settings, &session, &network, &listeningHistory, &listeningTracker);

    QObject::connect(&instance, &SingleInstance::commandReceived, &app, &AppController::handleCommand);
    instance.listen();

// qt finds d-bus on macos too, but mpris and logind only exist on linux
#if defined(SWIFTIFY_HAS_DBUS) && defined(Q_OS_LINUX)
    Mpris mpris(&player);
    QObject::connect(&mpris, &Mpris::raiseRequested, &app, &AppController::raiseRequested);
    QObject::connect(&mpris, &Mpris::quitRequested, &app, &AppController::quit);
    SleepMonitor sleepMonitor(&player);
#endif
#ifdef Q_OS_MACOS
    // the media keys, headphone buttons and control centre's now playing
    NowPlaying nowPlaying(&player, &artwork);
#endif
#if defined(SWIFTIFY_DESKTOP_INTEGRATION) && defined(SWIFTIFY_HAS_DBUS) && defined(Q_OS_LINUX)
    AudioDeviceMonitor devices;
    QObject::connect(&devices, &AudioDeviceMonitor::outputLost, &player, &PlaybackController::invalidateAudioOutput);
    QObject::connect(&devices, &AudioDeviceMonitor::outputArrived, &mpris, &Mpris::noteOutputArrived);
#endif

    QQmlApplicationEngine engine;
    QObject::connect(&translations, &Translations::languageChanged, &engine, &QQmlEngine::retranslate);
#ifdef Q_OS_WIN
    WindowsIntegration windowsIntegration(&player, &theme);
    engine.setInitialProperties({{QStringLiteral("visible"), false}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &windowsIntegration,
                     [&windowsIntegration](QObject *object, const QUrl &) {
        if (auto *window = qobject_cast<QWindow *>(object)) {
            windowsIntegration.setWindow(window);
            window->show();
        }
    });
#endif
    engine.addImageProvider(QStringLiteral("art"), new ArtworkImageProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &application,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("Swiftify", "Main");
    // the first launch carries out its own command too, once the window exists
    if (const QString command = SingleInstance::commandFrom(QCoreApplication::arguments()); command != QLatin1StringView("raise")) {
        app.handleCommand(command);
    }

    session.restoreSavedSessions();
    return application.exec();
}
