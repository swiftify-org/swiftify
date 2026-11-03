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

#include "app/ErrorCenter.h"
#include "app/Settings.h"
#include "app/Translations.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

using namespace swiftify;

class TranslationsTest : public QObject {
    Q_OBJECT
private slots:
    void catalogIsEmbeddedAndValid()
    {
        if (!kRussianTranslationEnabled) {
            QSKIP("Russian translation is disabled.");
        }
        QFile file(QStringLiteral(":/translations/ru.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(document.object().size() > 300);
        const auto catalog = document.object();
        for (auto it = catalog.begin(); it != catalog.end(); ++it) {
            if (it.value().isArray()) {
                QCOMPARE(it.value().toArray().size(), 3);
                for (const auto &value : it.value().toArray()) QVERIFY(!value.toString().isEmpty());
            } else {
                QVERIFY2(!it.value().toString().isEmpty(), qPrintable(it.key()));
            }
        }
    }

    void switchesLiveAndFallsBackToEnglish()
    {
        if (!kRussianTranslationEnabled) {
            QSKIP("Russian translation is disabled.");
        }
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("settings.ini")));
        settings.setLanguage(QStringLiteral("en"));
        Translations translations(&settings);
        QQmlEngine engine;
        connect(&translations, &Translations::languageChanged, &engine, &QQmlEngine::retranslate);
        QQmlComponent component(&engine);
        component.setData("import QtQml\nQtObject { property string label: qsTr(\"Settings\") }", QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("label").toString(), QStringLiteral("Settings"));
        settings.setLanguage(QStringLiteral("ru"));
        QCOMPARE(object->property("label").toString(), QStringLiteral("Настройки"));
        QCOMPARE(QCoreApplication::translate("test", "custom playlist"), QStringLiteral("custom playlist"));
        settings.setLanguage(QStringLiteral("en"));
        QCOMPARE(object->property("label").toString(), QStringLiteral("Settings"));
    }

    void russianPluralForms_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<QString>("expected");
        QTest::newRow("one") << 1 << QStringLiteral("1 трек");
        QTest::newRow("two") << 2 << QStringLiteral("2 трека");
        QTest::newRow("five") << 5 << QStringLiteral("5 треков");
        QTest::newRow("eleven") << 11 << QStringLiteral("11 треков");
        QTest::newRow("fourteen") << 14 << QStringLiteral("14 треков");
        QTest::newRow("twenty-one") << 21 << QStringLiteral("21 трек");
        QTest::newRow("twenty-two") << 22 << QStringLiteral("22 трека");
        QTest::newRow("zero") << 0 << QStringLiteral("0 треков");
    }

    void translatesFormattedErrorsWithoutChangingMetadata()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("settings.ini")));
        settings.setLanguage(QStringLiteral("ru"));
        Translations translations(&settings);
        QCOMPARE(QCoreApplication::translate("Errors", "Spotify token exchange failed: Connection refused"),
                 QStringLiteral("Не удалось получить токен Spotify: Соединение отклонено"));
        QCOMPARE(QCoreApplication::translate("Errors", "Soloist stopped (exit code 10). Connection refused"),
                 QStringLiteral("Soloist завершил работу с кодом 10. Соединение отклонено"));
        QCOMPARE(QCoreApplication::translate("Errors", "Spotify API request failed: /Users/test/Music"),
                 QStringLiteral("Ошибка запроса к API Spotify: /Users/test/Music"));
        QCOMPARE(QCoreApplication::translate("metadata", "Could not create a playlist"),
                 QStringLiteral("Could not create a playlist"));
        QCOMPARE(QCoreApplication::translate("Errors", "custom playlist"), QStringLiteral("custom playlist"));
        settings.setLanguage(QStringLiteral("en"));
        QCOMPARE(QCoreApplication::translate("Soloist", "Could not save the API key: Connection refused"),
                 QStringLiteral("Could not save the API key: Connection refused"));
    }

    void russianPluralForms()
    {
        if (!kRussianTranslationEnabled) {
            QSKIP("Russian translation is disabled.");
        }
        QFETCH(int, count);
        QFETCH(QString, expected);
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("settings.ini")));
        settings.setLanguage(QStringLiteral("ru"));
        Translations translations(&settings);
        QCOMPARE(QCoreApplication::translate("Format", "%n songs", nullptr, count), expected);
    }

    void germanCatalogIsEmbeddedAndValid()
    {
        if (!kGermanTranslationEnabled) {
            QSKIP("German translation is disabled.");
        }
        QFile file(QStringLiteral(":/translations/de.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        const auto catalog = document.object();
        QVERIFY(catalog.size() > 300);
        for (auto it = catalog.begin(); it != catalog.end(); ++it) {
            if (it.value().isArray()) {
                QCOMPARE(it.value().toArray().size(), 2);
                for (const auto &value : it.value().toArray()) QVERIFY(!value.toString().isEmpty());
            } else {
                QVERIFY2(!it.value().toString().isEmpty(), qPrintable(it.key()));
            }
        }
    }

    void switchesLiveToGermanAndBack()
    {
        if (!kGermanTranslationEnabled) {
            QSKIP("German translation is disabled.");
        }
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("settings.ini")));
        settings.setLanguage(QStringLiteral("en"));
        Translations translations(&settings);
        QQmlEngine engine;
        connect(&translations, &Translations::languageChanged, &engine, &QQmlEngine::retranslate);
        QQmlComponent component(&engine);
        component.setData("import QtQml\nQtObject { property string label: qsTr(\"Settings\") }", QUrl());
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("label").toString(), QStringLiteral("Settings"));
        settings.setLanguage(QStringLiteral("de"));
        QCOMPARE(object->property("label").toString(), QStringLiteral("Einstellungen"));
        QCOMPARE(QCoreApplication::translate("test", "custom playlist"), QStringLiteral("custom playlist"));
        settings.setLanguage(QStringLiteral("en"));
        QCOMPARE(object->property("label").toString(), QStringLiteral("Settings"));
    }

    void germanPluralForms_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<int>("count");
        QTest::addColumn<QString>("expected");
        QTest::newRow("songs-zero") << QStringLiteral("%n songs") << 0 << QStringLiteral("0 Titel");
        QTest::newRow("songs-one") << QStringLiteral("%n songs") << 1 << QStringLiteral("1 Titel");
        QTest::newRow("songs-two") << QStringLiteral("%n songs") << 2 << QStringLiteral("2 Titel");
        QTest::newRow("plays-zero") << QStringLiteral("%n plays") << 0 << QStringLiteral("0 Wiedergaben");
        QTest::newRow("plays-one") << QStringLiteral("%n plays") << 1 << QStringLiteral("1 Wiedergabe");
        QTest::newRow("plays-five") << QStringLiteral("%n plays") << 5 << QStringLiteral("5 Wiedergaben");
        QTest::newRow("days-one") << QStringLiteral("%n days") << 1 << QStringLiteral("1 Tag");
        QTest::newRow("days-three") << QStringLiteral("%n days") << 3 << QStringLiteral("3 Tage");
    }

    void germanPluralForms()
    {
        if (!kGermanTranslationEnabled) {
            QSKIP("German translation is disabled.");
        }
        QFETCH(QString, source);
        QFETCH(int, count);
        QFETCH(QString, expected);
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("settings.ini")));
        settings.setLanguage(QStringLiteral("de"));
        Translations translations(&settings);
        QCOMPARE(QCoreApplication::translate("Format", source.toUtf8().constData(), nullptr, count), expected);
    }

    void germanTranslatesFormattedErrorsWithoutChangingMetadata()
    {
        if (!kGermanTranslationEnabled) {
            QSKIP("German translation is disabled.");
        }
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("settings.ini")));
        settings.setLanguage(QStringLiteral("de"));
        Translations translations(&settings);
        QCOMPARE(QCoreApplication::translate("Errors", "Spotify API request failed: /Users/test/Music"),
                 QStringLiteral("Spotify-API-Anfrage fehlgeschlagen: /Users/test/Music"));
        QCOMPARE(QCoreApplication::translate("Errors", "custom playlist"), QStringLiteral("custom playlist"));
        QCOMPARE(QCoreApplication::translate("metadata", "Could not create a playlist"),
                 QStringLiteral("Could not create a playlist"));
    }

    void disabledLanguageFallsBackToEnglishAndShowsError()
    {
        const auto restore = qScopeGuard([] { Translations::resetLanguageEnabled(); });
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings.ini"));
        {
            QSettings raw(path, QSettings::IniFormat);
            raw.setValue(QStringLiteral("language"), QStringLiteral("ru"));
        }
        Translations::setLanguageEnabled(QStringLiteral("ru"), false);
        ErrorCenter errors(nullptr);
        Settings settings(path);
        Translations translations(&settings);
        QCOMPARE(settings.language(), QStringLiteral("en"));
        QCOMPARE(errors.message(),
                 QStringLiteral("this language is currently disabled due to missing language maintainers. your app language has been switched back to english."));
        QCOMPARE(QCoreApplication::translate("test", "Settings"), QStringLiteral("Settings"));

        errors.clear();
        Settings reopened(path);
        QCOMPARE(reopened.language(), QStringLiteral("en"));
        QVERIFY(errors.message().isEmpty());
    }

    void disabledGermanFallsBackToEnglishAndShowsError()
    {
        const auto restore = qScopeGuard([] { Translations::resetLanguageEnabled(); });
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings.ini"));
        {
            QSettings raw(path, QSettings::IniFormat);
            raw.setValue(QStringLiteral("language"), QStringLiteral("de"));
        }
        Translations::setLanguageEnabled(QStringLiteral("de"), false);
        ErrorCenter errors(nullptr);
        Settings settings(path);
        Translations translations(&settings);
        QCOMPARE(settings.language(), QStringLiteral("en"));
        QCOMPARE(errors.message(),
                 QStringLiteral("this language is currently disabled due to missing language maintainers. your app language has been switched back to english."));
        QCOMPARE(QCoreApplication::translate("test", "Settings"), QStringLiteral("Settings"));

        errors.clear();
        Settings reopened(path);
        QCOMPARE(reopened.language(), QStringLiteral("en"));
        QVERIFY(errors.message().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TranslationsTest)
#include "tst_translations.moc"
