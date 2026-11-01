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

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

/// the app's preferences, under the macos app's key names and defaults
/// (AppDesign.swift) so the two read the same way. stores that keep their own
/// data (volume, pins, search history, the equalizer) use value() and setValue().
class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool visualizersEnabled READ visualizersEnabled WRITE setVisualizersEnabled NOTIFY visualizersEnabledChanged FINAL)
    Q_PROPERTY(bool playerBarVisualizer READ playerBarVisualizer WRITE setPlayerBarVisualizer NOTIFY playerBarVisualizerChanged FINAL)
    Q_PROPERTY(bool trackListVisualizer READ trackListVisualizer WRITE setTrackListVisualizer NOTIFY trackListVisualizerChanged FINAL)
    Q_PROPERTY(bool immersiveVisualizer READ immersiveVisualizer WRITE setImmersiveVisualizer NOTIFY immersiveVisualizerChanged FINAL)
    Q_PROPERTY(bool circularVisualizer READ circularVisualizer WRITE setCircularVisualizer NOTIFY circularVisualizerChanged FINAL)
    Q_PROPERTY(bool continuousVisualizer READ continuousVisualizer WRITE setContinuousVisualizer NOTIFY continuousVisualizerChanged FINAL)
    /// the players' covers round, with their squiggle edge, while visualizers
    /// are off; with them on, the circular style rounds them
    Q_PROPERTY(bool roundArtwork READ roundArtwork WRITE setRoundArtwork NOTIFY roundArtworkChanged FINAL)
    Q_PROPERTY(QString visualizerShape READ visualizerShape WRITE setVisualizerShape NOTIFY visualizerShapeChanged FINAL)
    Q_PROPERTY(double visualizerSensitivity READ visualizerSensitivity WRITE setVisualizerSensitivity NOTIFY visualizerSensitivityChanged FINAL)
    Q_PROPERTY(int streamingQuality READ streamingQuality WRITE setStreamingQuality NOTIFY streamingQualityChanged FINAL)
    Q_PROPERTY(bool normalizeVolume READ normalizeVolume WRITE setNormalizeVolume NOTIFY normalizeVolumeChanged FINAL)
    Q_PROPERTY(bool spotifyConnectEnabled READ spotifyConnectEnabled WRITE setSpotifyConnectEnabled NOTIFY spotifyConnectEnabledChanged FINAL)
    Q_PROPERTY(bool crossfadeEnabled READ crossfadeEnabled WRITE setCrossfadeEnabled NOTIFY crossfadeEnabledChanged FINAL)
    Q_PROPERTY(int crossfadeDurationSeconds READ crossfadeDurationSeconds WRITE setCrossfadeDurationSeconds NOTIFY crossfadeDurationSecondsChanged FINAL)
    /// how much of the songs streamed is kept, so playing one again needs no
    /// download: 500, 1000 or 2000 megabytes
    Q_PROPERTY(int songCacheMegabytes READ songCacheMegabytes WRITE setSongCacheMegabytes NOTIFY songCacheMegabytesChanged FINAL)
    Q_PROPERTY(bool livingBackdrop READ livingBackdrop WRITE setLivingBackdrop NOTIFY livingBackdropChanged FINAL)
    Q_PROPERTY(double backdropBlur READ backdropBlur WRITE setBackdropBlur NOTIFY backdropBlurChanged FINAL)
    Q_PROPERTY(double backdropReactivity READ backdropReactivity WRITE setBackdropReactivity NOTIFY backdropReactivityChanged FINAL)
    Q_PROPERTY(bool debuggingEnabled READ debuggingEnabled WRITE setDebuggingEnabled NOTIFY debuggingEnabledChanged FINAL)
    Q_PROPERTY(bool pinnedPlaylistsExpanded READ pinnedPlaylistsExpanded WRITE setPinnedPlaylistsExpanded NOTIFY pinnedPlaylistsExpandedChanged FINAL)
    Q_PROPERTY(bool allPlaylistsExpanded READ allPlaylistsExpanded WRITE setAllPlaylistsExpanded NOTIFY allPlaylistsExpandedChanged FINAL)
    Q_PROPERTY(QString immersivePlayerPanel READ immersivePlayerPanel WRITE setImmersivePlayerPanel NOTIFY immersivePlayerPanelChanged FINAL)
    Q_PROPERTY(QString sleepTimerFade READ sleepTimerFade WRITE setSleepTimerFade NOTIFY sleepTimerFadeChanged FINAL)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged FINAL)
    Q_PROPERTY(bool performanceMode READ performanceMode WRITE setPerformanceMode NOTIFY performanceModeChanged FINAL)
    /// no shapes behind the pages or the immersive player, and every other
    /// shape a plain circle, but for the favourite songs' heart
    Q_PROPERTY(bool noShapes READ noShapes WRITE setNoShapes NOTIFY noShapesChanged FINAL)
    /// "system" (a Matugen scheme), "builtin", or a theme id
    Q_PROPERTY(QString themeSource READ themeSource WRITE setThemeSource NOTIFY themeSourceChanged FINAL)
    /// "system", "light" or "dark"; selects a variant when Matugen provides both
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged FINAL)
    Q_PROPERTY(QString matugenColorsPath READ matugenColorsPath WRITE setMatugenColorsPath NOTIFY matugenColorsPathChanged FINAL)
    Q_PROPERTY(bool showWindowControls READ showWindowControls WRITE setShowWindowControls NOTIFY showWindowControlsChanged FINAL)
    Q_PROPERTY(bool windowControlsOnRight READ windowControlsOnRight WRITE setWindowControlsOnRight NOTIFY windowControlsOnRightChanged FINAL)
    /// where the stats page reads from: "statsfm" or "local"
    Q_PROPERTY(QString statsSource READ statsSource WRITE setStatsSource NOTIFY statsSourceChanged FINAL)
    Q_PROPERTY(QString statsFmUsername READ statsFmUsername WRITE setStatsFmUsername NOTIFY statsFmUsernameChanged FINAL)
    /// whether songs played here are kept for the local stats
    Q_PROPERTY(bool trackListening READ trackListening WRITE setTrackListening NOTIFY trackListeningChanged FINAL)
    Q_PROPERTY(bool nativeTitleBar READ nativeTitleBar WRITE setNativeTitleBar NOTIFY nativeTitleBarChanged FINAL)
    Q_PROPERTY(bool trayIcon READ trayIcon WRITE setTrayIcon NOTIFY trayIconChanged FINAL)
    /// the whole window's zoom, as ctrl + and ctrl - set it
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged FINAL)
    Q_PROPERTY(bool discordEnabled READ discordEnabled WRITE setDiscordEnabled NOTIFY discordEnabledChanged FINAL)
    Q_PROPERTY(QString discordApplicationId READ discordApplicationId WRITE setDiscordApplicationId NOTIFY discordApplicationIdChanged FINAL)
    Q_PROPERTY(bool loggingEnabled READ isLoggingEnabled WRITE setLoggingEnabled NOTIFY loggingEnabledChanged FINAL)
    Q_PROPERTY(bool loggingActive READ isLoggingActive CONSTANT FINAL)
    Q_PROPERTY(QString logDirectory READ logDirectory CONSTANT FINAL)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT FINAL)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged FINAL)
    Q_PROPERTY(bool russianEnabled READ isRussianEnabled NOTIFY languageAvailabilityChanged FINAL)
    Q_PROPERTY(QString disabledLanguageMessage READ disabledLanguageMessage CONSTANT FINAL)

public:
    /// the default file, ~/.config/Swiftify/Swiftify.conf. no default argument:
    /// qml builds a singleton with a default constructor in preference to
    /// create(), and would end up with an instance of its own
    explicit Settings(QObject *parent);
    /// a specific file, for tests
    explicit Settings(const QString &path, QObject *parent = nullptr);
    ~Settings() override;

    static Settings *instance();
    static Settings *create(QQmlEngine *, QJSEngine *);

    QVariant value(const QString &key, const QVariant &fallback = {}) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);
    bool contains(const QString &key) const;
    void sync();

    /// every audio visualizer in the app
    bool visualizersEnabled() const { return m_visualizersEnabled; }
    void setVisualizersEnabled(bool value);
    bool playerBarVisualizer() const { return m_playerBarVisualizer; }
    void setPlayerBarVisualizer(bool value);
    bool trackListVisualizer() const { return m_trackListVisualizer; }
    void setTrackListVisualizer(bool value);
    bool immersiveVisualizer() const { return m_immersiveVisualizer; }
    void setImmersiveVisualizer(bool value);
    bool anyVisualizerEnabled() const
    {
        return m_visualizersEnabled && (m_playerBarVisualizer || m_trackListVisualizer || m_immersiveVisualizer);
    }
    /// ring around round artwork instead of a band of bars
    bool circularVisualizer() const { return m_circularVisualizer; }
    void setCircularVisualizer(bool value);
    /// the immersive band as one row from bass to treble instead of mirrored
    bool continuousVisualizer() const { return m_continuousVisualizer; }
    void setContinuousVisualizer(bool value);
    bool roundArtwork() const { return m_roundArtwork; }
    void setRoundArtwork(bool value);
    /// bars or wave. stored under its first name, from when it applied to the ring alone
    QString visualizerShape() const { return m_visualizerShape; }
    void setVisualizerShape(const QString &value);
    double visualizerSensitivity() const { return m_visualizerSensitivity; }
    void setVisualizerSensitivity(double value);
    /// kbps: 96, 160 or 320
    int streamingQuality() const { return m_streamingQuality; }
    int songCacheMegabytes() const { return m_songCacheMegabytes; }
    void setSongCacheMegabytes(int megabytes);
    void setStreamingQuality(int value);
    bool normalizeVolume() const { return m_normalizeVolume; }
    void setNormalizeVolume(bool value);
    bool spotifyConnectEnabled() const { return m_spotifyConnectEnabled; }
    void setSpotifyConnectEnabled(bool value);
    bool crossfadeEnabled() const { return m_crossfadeEnabled; }
    void setCrossfadeEnabled(bool value);
    int crossfadeDurationSeconds() const { return m_crossfadeDurationSeconds; }
    void setCrossfadeDurationSeconds(int value);
    /// animate the immersive background with the music
    bool livingBackdrop() const { return m_livingBackdrop; }
    void setLivingBackdrop(bool value);
    double backdropBlur() const { return m_backdropBlur; }
    void setBackdropBlur(double value);
    double backdropReactivity() const { return m_backdropReactivity; }
    void setBackdropReactivity(double value);
    /// adds a seconds field to the sleep timer
    bool debuggingEnabled() const { return m_debuggingEnabled; }
    void setDebuggingEnabled(bool value);
    bool pinnedPlaylistsExpanded() const { return m_pinnedPlaylistsExpanded; }
    void setPinnedPlaylistsExpanded(bool value);
    bool allPlaylistsExpanded() const { return m_allPlaylistsExpanded; }
    void setAllPlaylistsExpanded(bool value);
    QString immersivePlayerPanel() const { return m_immersivePlayerPanel; }
    void setImmersivePlayerPanel(const QString &value);
    QString sleepTimerFade() const { return m_sleepTimerFade; }
    void setSleepTimerFade(const QString &value);
    /// linux only. seeded from the desktop's animation setting on first launch
    bool reduceMotion() const { return m_reduceMotion; }
    void setReduceMotion(bool value);
    /// as little drawn as can be: nothing moves, fades or turns that does not
    /// have to, beyond reduced motion's fades
    bool performanceMode() const { return m_performanceMode; }
    void setPerformanceMode(bool value);
    bool noShapes() const { return m_noShapes; }
    void setNoShapes(bool value);
    /// system follows a discovered or explicitly selected Matugen scheme
    QString themeSource() const { return m_themeSource; }
    void setThemeSource(const QString &value);
    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString &value);
    /// empty discovers Matugen JSON in common config/cache locations
    QString matugenColorsPath() const { return m_matugenColorsPath; }
    void setMatugenColorsPath(const QString &value);
    /// linux only. minimise, maximise and close in the top bar
    bool showWindowControls() const { return m_showWindowControls; }
    void setShowWindowControls(bool value);
    bool windowControlsOnRight() const { return m_windowControlsOnRight; }
    QString statsSource() const { return m_statsSource; }
    void setStatsSource(const QString &value);
    QString statsFmUsername() const { return m_statsFmUsername; }
    void setStatsFmUsername(const QString &value);
    bool trackListening() const { return m_trackListening; }
    void setTrackListening(bool value);
    void setWindowControlsOnRight(bool value);
    bool nativeTitleBar() const { return m_nativeTitleBar; }
    void setNativeTitleBar(bool value);
    /// linux only
    bool trayIcon() const { return m_trayIcon; }
    void setTrayIcon(bool value);
    double zoom() const { return m_zoom; }
    void setZoom(double value);
    bool discordEnabled() const { return m_discordEnabled; }
    void setDiscordEnabled(bool value);
    QString discordApplicationId() const { return m_discordApplicationId; }
    void setDiscordApplicationId(const QString &value);
    QString language() const { return m_language; }
    void setLanguage(const QString &value);
    bool isLoggingEnabled() const { return m_loggingEnabled; }
    void setLoggingEnabled(bool value);
    bool isLoggingActive() const;
    QString logDirectory() const;
    QString logFilePath() const;
    Q_INVOKABLE void openLogsFolder() const;
    Q_INVOKABLE void openLogFile() const;
    bool isRussianEnabled() const;
    QString disabledLanguageMessage() const;
    Q_INVOKABLE bool isLanguageEnabled(const QString &language) const;
    void enforceEnabledLanguage();
    bool takeRevertedDisabledLanguage();

signals:
    void visualizersEnabledChanged();
    void playerBarVisualizerChanged();
    void trackListVisualizerChanged();
    void immersiveVisualizerChanged();
    void circularVisualizerChanged();
    void continuousVisualizerChanged();
    void roundArtworkChanged();
    void visualizerShapeChanged();
    void visualizerSensitivityChanged();
    void streamingQualityChanged();
    void normalizeVolumeChanged();
    void spotifyConnectEnabledChanged();
    void crossfadeEnabledChanged();
    void crossfadeDurationSecondsChanged();
    void songCacheMegabytesChanged();
    void livingBackdropChanged();
    void backdropBlurChanged();
    void backdropReactivityChanged();
    void debuggingEnabledChanged();
    void pinnedPlaylistsExpandedChanged();
    void allPlaylistsExpandedChanged();
    void immersivePlayerPanelChanged();
    void sleepTimerFadeChanged();
    void reduceMotionChanged();
    void performanceModeChanged();
    void noShapesChanged();
    void themeSourceChanged();
    void themeModeChanged();
    void matugenColorsPathChanged();
    void showWindowControlsChanged();
    void windowControlsOnRightChanged();
    void statsSourceChanged();
    void statsFmUsernameChanged();
    void trackListeningChanged();
    void nativeTitleBarChanged();
    void trayIconChanged();
    void zoomChanged();
    void discordEnabledChanged();
    void discordApplicationIdChanged();
    void loggingEnabledChanged();
    void languageChanged();
    void languageAvailabilityChanged();

private:
    void load();

    QSettings m_store;

    bool m_visualizersEnabled = true;
    bool m_playerBarVisualizer = true;
    bool m_trackListVisualizer = true;
    bool m_immersiveVisualizer = true;
    bool m_circularVisualizer = false;
    bool m_continuousVisualizer = false;
    bool m_roundArtwork = false;
    QString m_visualizerShape = QStringLiteral("bars");
    double m_visualizerSensitivity = 1.0;
    int m_streamingQuality = 160;
    bool m_normalizeVolume = false;
    bool m_spotifyConnectEnabled = false;
    bool m_crossfadeEnabled = false;
    int m_crossfadeDurationSeconds = 5;
    int m_songCacheMegabytes = 1000;
    bool m_livingBackdrop = true;
    double m_backdropBlur = 16.0;
    double m_backdropReactivity = 0.3;
    bool m_debuggingEnabled = false;
    bool m_pinnedPlaylistsExpanded = false;
    bool m_allPlaylistsExpanded = false;
    QString m_immersivePlayerPanel = QStringLiteral("lyrics");
    QString m_sleepTimerFade = QStringLiteral("finalSeconds");
    bool m_reduceMotion = false;
    bool m_performanceMode = false;
    bool m_noShapes = false;
    QString m_themeSource = QStringLiteral("spotify");
    QString m_themeMode = QStringLiteral("system");
    QString m_matugenColorsPath = QString();
    bool m_showWindowControls = true;
    bool m_windowControlsOnRight = false;
    QString m_statsSource = QStringLiteral("local");
    QString m_statsFmUsername;
    bool m_trackListening = true;
#ifdef Q_OS_WIN
    bool m_nativeTitleBar = false;
#else
    bool m_nativeTitleBar = true;
#endif
    bool m_trayIcon = true;
    double m_zoom = 1.0;
    bool m_discordEnabled = false;
    QString m_discordApplicationId;
    QString m_language = QStringLiteral("system");
    bool m_loggingEnabled = false;
    bool m_revertedDisabledLanguage = false;
};

} // namespace swiftify
