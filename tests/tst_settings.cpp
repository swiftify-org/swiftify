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

#include "app/Settings.h"
#include "app/Translations.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using swiftify::Settings;

class SettingsTest : public QObject {
    Q_OBJECT

private slots:
    void spotifyConnectIsOptInAndPersists()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            QVERIFY(!settings.spotifyConnectEnabled());
            QSignalSpy changed(&settings, &Settings::spotifyConnectEnabledChanged);
            settings.setSpotifyConnectEnabled(true);
            settings.setSpotifyConnectEnabled(true);
            QCOMPARE(changed.count(), 1);
        }
        {
            Settings settings(path);
            QVERIFY(settings.spotifyConnectEnabled());
            settings.setSpotifyConnectEnabled(false);
        }
        Settings reopened(path);
        QVERIFY(!reopened.spotifyConnectEnabled());
    }

    void discordPreferencesPersist()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            QVERIFY(!settings.discordEnabled());
            QVERIFY(settings.discordApplicationId().isEmpty());
            QSignalSpy discord(&settings, &Settings::discordEnabledChanged);
            QSignalSpy id(&settings, &Settings::discordApplicationIdChanged);
            settings.setDiscordEnabled(true);
            settings.setDiscordEnabled(true);
            QCOMPARE(discord.count(), 1);
            settings.setDiscordApplicationId(QStringLiteral(" 123456789012345678 "));
            settings.setDiscordApplicationId(QStringLiteral("invalid"));
            QCOMPARE(id.count(), 1);
            QCOMPARE(settings.discordApplicationId(), QStringLiteral("123456789012345678"));
        }
        Settings reopened(path);
        QVERIFY(reopened.discordEnabled());
        QCOMPARE(reopened.discordApplicationId(), QStringLiteral("123456789012345678"));
        reopened.setDiscordApplicationId(QString());
        QVERIFY(reopened.discordApplicationId().isEmpty());
    }

    void languagePreferencePersists()
    {
        if (!swiftify::kRussianTranslationEnabled) {
            QSKIP("Russian translation is disabled.");
        }
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            QCOMPARE(settings.language(), QStringLiteral("system"));
            QSignalSpy language(&settings, &Settings::languageChanged);
            settings.setLanguage(QStringLiteral("ru"));
            settings.setLanguage(QStringLiteral("ru"));
            QCOMPARE(language.count(), 1);
        }
        Settings reopened(path);
        QCOMPARE(reopened.language(), QStringLiteral("ru"));
        reopened.setLanguage(QStringLiteral("unknown"));
        QCOMPARE(reopened.language(), QStringLiteral("system"));
    }

    void loggingPreferencePersists()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            QVERIFY(!settings.isLoggingEnabled());
            QSignalSpy loggingSpy(&settings, &Settings::loggingEnabledChanged);
            settings.setLoggingEnabled(true);
            settings.setLoggingEnabled(true);
            QCOMPARE(loggingSpy.count(), 1);
            QVERIFY(settings.isLoggingEnabled());
        }
        Settings reopened(path);
        QVERIFY(reopened.isLoggingEnabled());
        reopened.setLoggingEnabled(false);
        QVERIFY(!reopened.isLoggingEnabled());
    }

    void defaultsMatchTheMacApp()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        QCOMPARE(settings.isLoggingEnabled(), false);
        QCOMPARE(settings.visualizersEnabled(), true);
        QCOMPARE(settings.playerBarVisualizer(), true);
        QCOMPARE(settings.trackListVisualizer(), true);
        QCOMPARE(settings.immersiveVisualizer(), true);
        QCOMPARE(settings.circularVisualizer(), false);
        QCOMPARE(settings.continuousVisualizer(), false);
        QCOMPARE(settings.visualizerShape(), QStringLiteral("bars"));
        QCOMPARE(settings.visualizerSensitivity(), 1.0);
        QCOMPARE(settings.streamingQuality(), 160);
        QCOMPARE(settings.normalizeVolume(), false);
        QCOMPARE(settings.songCacheMegabytes(), 1000);
        QCOMPARE(settings.livingBackdrop(), true);
        QCOMPARE(settings.backdropBlur(), 16.0);
        QCOMPARE(settings.backdropReactivity(), 0.3);
        QCOMPARE(settings.immersivePlayerPanel(), QStringLiteral("lyrics"));
        QCOMPARE(settings.sleepTimerFade(), QStringLiteral("finalSeconds"));
        QCOMPARE(settings.themeSource(), QStringLiteral("spotify"));
        QCOMPARE(settings.themeMode(), QStringLiteral("system"));
        QCOMPARE(settings.windowControlsOnRight(), false);
#ifdef Q_OS_WIN
        QVERIFY(!settings.nativeTitleBar());
#else
        QVERIFY(settings.nativeTitleBar());
#endif
    }

    void savedThemeOverridesTheDefault_data()
    {
        QTest::addColumn<QString>("source");
        for (const char *source : {"system", "builtin", "spotify", "catppuccin-mocha"}) {
            QTest::newRow(source) << QString::fromLatin1(source);
        }
    }

    void savedThemeOverridesTheDefault()
    {
        QFETCH(QString, source);
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            QSettings store(path, QSettings::IniFormat);
            store.setValue(QStringLiteral("themeSource"), source);
        }
        Settings settings(path);
        QCOMPARE(settings.themeSource(), source);
    }

    void titleBarPreferencePersists_data()
    {
        QTest::addColumn<bool>("nativeTitleBar");
        QTest::newRow("native") << true;
        QTest::newRow("in-app") << false;
    }

    void titleBarPreferencePersists()
    {
        QFETCH(bool, nativeTitleBar);
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            settings.setNativeTitleBar(!nativeTitleBar);
            QSignalSpy changed(&settings, &Settings::nativeTitleBarChanged);
            settings.setNativeTitleBar(nativeTitleBar);
            settings.setNativeTitleBar(nativeTitleBar);
            QCOMPARE(changed.count(), 1);
        }
        Settings reopened(path);
        QCOMPARE(reopened.nativeTitleBar(), nativeTitleBar);
    }

    void valuesAreSanitized()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        settings.setVisualizerSensitivity(4.0);
        QCOMPARE(settings.visualizerSensitivity(), 1.8);
        settings.setStreamingQuality(128);
        QCOMPARE(settings.streamingQuality(), 160);
        settings.setVisualizerShape(QStringLiteral("spiral"));
        QCOMPARE(settings.visualizerShape(), QStringLiteral("bars"));
        settings.setBackdropBlur(-3);
        QCOMPARE(settings.backdropBlur(), 0.0);
        // only the sizes offered: anything else is the default
        settings.setSongCacheMegabytes(750);
        QCOMPARE(settings.songCacheMegabytes(), 1000);
    }

    void changesNotifyOnceAndPersistUnderTheMacKeys()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            QSignalSpy spy(&settings, &Settings::visualizerShapeChanged);
            settings.setVisualizerShape(QStringLiteral("wave"));
            settings.setVisualizerShape(QStringLiteral("wave"));
            QCOMPARE(spy.count(), 1);
            QSignalSpy playerBarSpy(&settings, &Settings::playerBarVisualizerChanged);
            settings.setPlayerBarVisualizer(false);
            settings.setPlayerBarVisualizer(false);
            QCOMPARE(playerBarSpy.count(), 1);
            QSignalSpy trackListSpy(&settings, &Settings::trackListVisualizerChanged);
            settings.setTrackListVisualizer(false);
            settings.setTrackListVisualizer(false);
            QCOMPARE(trackListSpy.count(), 1);
            QSignalSpy immersiveSpy(&settings, &Settings::immersiveVisualizerChanged);
            settings.setImmersiveVisualizer(false);
            settings.setImmersiveVisualizer(false);
            QCOMPARE(immersiveSpy.count(), 1);
            QSignalSpy continuousSpy(&settings, &Settings::continuousVisualizerChanged);
            settings.setContinuousVisualizer(true);
            settings.setContinuousVisualizer(true);
            QCOMPARE(continuousSpy.count(), 1);
            settings.setStreamingQuality(96);
            QSignalSpy normalizeSpy(&settings, &Settings::normalizeVolumeChanged);
            settings.setNormalizeVolume(true);
            settings.setNormalizeVolume(true);
            QCOMPARE(normalizeSpy.count(), 1);
            settings.setSongCacheMegabytes(2000);
            settings.setPerformanceMode(true);
            settings.setNoShapes(true);
        }
        QSettings raw(path, QSettings::IniFormat);
        QCOMPARE(raw.value(QStringLiteral("circularVisualizerShape")).toString(), QStringLiteral("wave"));
        QCOMPARE(raw.value(QStringLiteral("playerBarVisualizer")).toBool(), false);
        QCOMPARE(raw.value(QStringLiteral("trackListVisualizer")).toBool(), false);
        QCOMPARE(raw.value(QStringLiteral("immersiveVisualizer")).toBool(), false);
        QCOMPARE(raw.value(QStringLiteral("continuousVisualizer")).toBool(), true);

        Settings reopened(path);
        QCOMPARE(reopened.visualizerShape(), QStringLiteral("wave"));
        QCOMPARE(reopened.playerBarVisualizer(), false);
        QCOMPARE(reopened.trackListVisualizer(), false);
        QCOMPARE(reopened.immersiveVisualizer(), false);
        QCOMPARE(reopened.continuousVisualizer(), true);
        QCOMPARE(reopened.streamingQuality(), 96);
        QCOMPARE(reopened.normalizeVolume(), true);
        QCOMPARE(reopened.songCacheMegabytes(), 2000);
        QVERIFY(reopened.performanceMode());
        QVERIFY(reopened.noShapes());
        QVERIFY(!reopened.reduceMotion());
    }

    void windowControlsOnRightPersists()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            QSignalSpy spy(&settings, &Settings::windowControlsOnRightChanged);
            settings.setWindowControlsOnRight(true);
            settings.setWindowControlsOnRight(true);
            QCOMPARE(spy.count(), 1);
            QVERIFY(settings.windowControlsOnRight());
        }
        Settings reopened(path);
        QVERIFY(reopened.windowControlsOnRight());
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "tst_settings.moc"
