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
#include "theme/CustomThemes.h"
#include "theme/Theme.h"
#include "theme/Themes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace swiftify;

namespace {

const QString kGoodId = QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174000");

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

int jsonFiles(const QString &directory)
{
    return QDir(directory).entryList({QStringLiteral("*.json")}, QDir::Files).size();
}

QString storedThemePath(const QString &directory, const QString &name, const QString &id)
{
    return QDir(directory).filePath(name + QStringLiteral("--") + id + QStringLiteral(".json"));
}

QString optionName(const Theme &theme, const QString &id)
{
    for (const ThemeOption &option : theme.themeOptions(true)) {
        if (option.id == id) {
            return option.name;
        }
    }
    return {};
}

} // namespace

class CustomThemesTest : public QObject {
    Q_OBJECT

private slots:
    // --- the store, on a folder of its own ---------------------------------

    void survivesARestart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString id;
        {
            CustomThemes store(dir.path());
            QVERIFY(store.beginNew(QStringLiteral("nord")));
            store.setDraftName(QStringLiteral("Mine"));
            QVERIFY(store.setDraftColor(true, QStringLiteral("primary"), QColor(0x11, 0x22, 0x33)));
            id = store.saveDraft();
            QVERIFY(id.startsWith(QStringLiteral("custom-")));
        }
        CustomThemes again(dir.path());
        const themes::Family *family = again.find(id);
        QVERIFY(family);
        QCOMPARE(family->name, QStringLiteral("Mine"));
        QCOMPARE(family->dark.primary, QRgb(0xff112233));
        QCOMPARE(family->light.window, themes::find(QStringLiteral("nord"))->light.window);
    }

    void skipsBrokenFiles()
    {
        QTemporaryDir dir;
        const QByteArray good = R"({"schema":1,"name":"Good"})";
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Good"), kGoodId), good));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-not-a-uuid.json")), good));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174000.backup.json")), good));
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Uppercase"),
                                          QStringLiteral("custom-123E4567-E89B-42D3-A456-426614174009")), good));
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Broken"),
                                          QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174001")), "{ nope"));
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Oversized"),
                                          QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174002")),
                          QByteArray(300 * 1024, ' ') + good));
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Future"),
                                          QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174003")),
                          R"({"schema":2,"name":"Future"})"));
        CustomThemes store(dir.path());
        QCOMPARE(store.families().size(), 1);
        QCOMPARE(store.families().first().id, kGoodId);
    }

    void duplicateIdsLoadTheFirstValidFileByName()
    {
        QTemporaryDir dir;
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Alpha"), kGoodId),
                          R"({"schema":1,"name":"Alpha theme"})"));
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Zulu"), kGoodId),
                          R"({"schema":1,"name":"Zulu theme"})"));

        CustomThemes store(dir.path());

        QCOMPARE(store.families().size(), 1);
        QCOMPARE(store.families().first().id, kGoodId);
        QCOMPARE(store.families().first().name, QStringLiteral("Alpha theme"));
    }

    void rejectsBadThemeFiles_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("no schema") << QByteArray(R"({"name":"X"})");
        QTest::newRow("future schema") << QByteArray(R"({"schema":2,"name":"X"})");
        QTest::newRow("no name") << QByteArray(R"({"schema":1})");
        QTest::newRow("not json") << QByteArray("{ nope");
        QTest::newRow("not an object") << QByteArray("[1, 2]");
        QTest::newRow("colour name") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":"red"}})");
        QTest::newRow("short hex") << QByteArrayLiteral("{\"schema\":1,\"name\":\"X\",\"dark\":{\"primary\":\"#abc\"}}");
        QTest::newRow("no hash") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":"aabbcc"}})");
        QTest::newRow("alpha hex") << QByteArrayLiteral("{\"schema\":1,\"name\":\"X\",\"dark\":{\"primary\":\"#aabbccdd\"}}");
        QTest::newRow("colour not a string") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":123}})");
        QTest::newRow("variant not an object") << QByteArray(R"({"schema":1,"name":"X","dark":[]})");
    }

    void rejectsBadThemeFiles()
    {
        QFETCH(QByteArray, json);
        QTemporaryDir dir;
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Invalid"), kGoodId), json));
        CustomThemes store(dir.path());
        QCOMPARE(store.families().size(), 0);
    }

    void fillsMissingColorsFromBaseTheme()
    {
        QTemporaryDir dir;
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("X"), kGoodId),
                          QByteArrayLiteral("{\"schema\":1,\"name\":\"X\",\"base\":\"nord\",\"dark\":{\"primary\":\"#010203\"},\"unknown\":1}")));
        CustomThemes store(dir.path());
        const themes::Family *family = store.find(kGoodId);
        QVERIFY(family);
        const themes::Family *nord = themes::find(QStringLiteral("nord"));
        QCOMPARE(family->dark.primary, QRgb(0xff010203));
        QCOMPARE(family->dark.window, nord->dark.window);
        QCOMPARE(family->light.primary, nord->light.primary);
    }

    void hexIsExactlySixDigits()
    {
        QVERIFY(CustomThemes::parseHex(QStringLiteral("#aabbcc")).isValid());
        QVERIFY(CustomThemes::parseHex(QStringLiteral("aabbcc")).isValid());
        QVERIFY(CustomThemes::parseHex(QStringLiteral(" AABBCC ")).isValid());
        for (const QString &bad : {QString(), QStringLiteral("#abc"), QStringLiteral("#aabbccdd"), QStringLiteral("red"),
                                   QStringLiteral("#gg0000"), QStringLiteral("##aabbcc"), QStringLiteral("#aabbc"),
                                   QStringLiteral("#aabb cc"), QStringLiteral("#aabbcc\n#aabbcc")}) {
            QVERIFY2(!CustomThemes::parseHex(bad).isValid(), qPrintable(bad));
        }
        QCOMPARE(CustomThemes::hex(QColor(0xAA, 0xBB, 0xCC)), QStringLiteral("#aabbcc"));
        QCOMPARE(CustomThemes::hex(QColor(0xAA, 0xBB, 0xCC, 0x10)), QStringLiteral("#aabbcc"));
    }

    void draftColoursAndReset()
    {
        QTemporaryDir dir;
        CustomThemes store(dir.path());
        QVERIFY(store.beginNew(QStringLiteral("spotify")));
        QVERIFY(store.setDraftColorHex(false, QStringLiteral("primary"), QStringLiteral("#102030")));
        QCOMPARE(store.draftColor(false, QStringLiteral("primary")), QColor(0x10, 0x20, 0x30));
        QSignalSpy errors(&store, &CustomThemes::errorChanged);
        for (const QString &bad : {QStringLiteral("blue"), QStringLiteral("#abc"), QStringLiteral("#12345678"), QString()}) {
            QVERIFY(!store.setDraftColorHex(false, QStringLiteral("primary"), bad));
        }
        QCOMPARE(errors.count(), 4);
        QCOMPARE(store.draftColor(false, QStringLiteral("primary")), QColor(0x10, 0x20, 0x30));
        QVERIFY(!store.setDraftColor(false, QStringLiteral("noSuchKey"), QColor(Qt::red)));
        QVERIFY(!store.setDraftColor(false, QStringLiteral("primary"), QColor()));
        store.resetDraftColor(false, QStringLiteral("primary"));
        QCOMPARE(store.draftColor(false, QStringLiteral("primary")),
                 QColor::fromRgba(themes::find(QStringLiteral("spotify"))->light.primary));
        // light and dark are kept apart
        QVERIFY(store.setDraftColor(true, QStringLiteral("primary"), QColor(1, 2, 3)));
        QCOMPARE(store.draftColor(false, QStringLiteral("primary")),
                 QColor::fromRgba(themes::find(QStringLiteral("spotify"))->light.primary));
        store.resetDraft();
        QCOMPARE(store.draftColor(true, QStringLiteral("primary")),
                 QColor::fromRgba(themes::find(QStringLiteral("spotify"))->dark.primary));
    }

    void discardLeavesNothingBehind()
    {
        QTemporaryDir dir;
        CustomThemes store(dir.path());
        QVERIFY(store.beginNew(QStringLiteral("default")));
        QVERIFY(store.editing());
        QVERIFY(store.dirty());
        store.discardDraft();
        QVERIFY(!store.editing());
        QVERIFY(!store.draft());
        QCOMPARE(store.families().size(), 0);
        QCOMPARE(jsonFiles(dir.path()), 0);
    }

    void editingASavedThemeIsCleanUntilChanged()
    {
        QTemporaryDir dir;
        CustomThemes store(dir.path());
        QVERIFY(store.beginNew(QStringLiteral("default")));
        const QString id = store.saveDraft();
        QVERIFY(store.beginEdit(id));
        QVERIFY(!store.dirty());
        store.setDraftName(QStringLiteral("Renamed"));
        QVERIFY(store.dirty());
        const int revision = store.revision();
        QCOMPARE(store.saveDraft(), id);
        QVERIFY(store.revision() > revision);
        QCOMPARE(store.find(id)->name, QStringLiteral("Renamed"));
        QCOMPARE(store.families().size(), 1);
        QCOMPARE(jsonFiles(dir.path()), 1);
    }

    void failedRenameSaveKeepsThePreviousFile()
    {
        QTemporaryDir dir;
        CustomThemes store(dir.path());
        QVERIFY(store.beginNew(QStringLiteral("default")));
        const QString id = store.saveDraft();
        QVERIFY(!id.isEmpty());

        const QString oldPath = storedThemePath(dir.path(), QStringLiteral("My theme"), id);
        QFile oldFile(oldPath);
        QVERIFY(oldFile.open(QIODevice::ReadOnly));
        const QByteArray oldContents = oldFile.readAll();
        oldFile.close();

        const QString blockedTarget = storedThemePath(dir.path(), QStringLiteral("Renamed"), id);
        QVERIFY(QDir().mkpath(blockedTarget));
        QVERIFY(store.beginEdit(id));
        store.setDraftName(QStringLiteral("Renamed"));

        QVERIFY(store.saveDraft().isEmpty());
        QVERIFY(store.editing());
        QCOMPARE(store.find(id)->name, QStringLiteral("My theme"));
        QVERIFY(QFileInfo::exists(oldPath));
        QVERIFY(QFileInfo(blockedTarget).isDir());

        QFile unchangedFile(oldPath);
        QVERIFY(unchangedFile.open(QIODevice::ReadOnly));
        QCOMPARE(unchangedFile.readAll(), oldContents);
    }

    void saveNeedsAName()
    {
        QTemporaryDir dir;
        CustomThemes store(dir.path());
        QVERIFY(store.beginNew(QStringLiteral("default")));
        store.setDraftName(QStringLiteral("   "));
        QSignalSpy errors(&store, &CustomThemes::errorChanged);
        QVERIFY(store.saveDraft().isEmpty());
        QVERIFY(!store.lastError().isEmpty());
        QCOMPARE(errors.count(), 1);
        QVERIFY(store.editing());
        QCOMPARE(jsonFiles(dir.path()), 0);
    }

    void builtinsAreProtected()
    {
        QTemporaryDir dir;
        CustomThemes store(dir.path());
        QVERIFY(!store.find(QStringLiteral("nord")));
        QVERIFY(!store.beginEdit(QStringLiteral("nord")));
        QVERIFY(!store.remove(QStringLiteral("nord")));
        const QString copy = store.duplicate(QStringLiteral("nord"));
        QVERIFY(copy.startsWith(QStringLiteral("custom-")));
        QCOMPARE(store.find(copy)->dark.primary, themes::find(QStringLiteral("nord"))->dark.primary);
        QVERIFY(themes::find(copy) == nullptr);
        QVERIFY(themes::find(QStringLiteral("nord")) != nullptr);
    }

    void removeDeletesTheFileAndEndsEditing()
    {
        QTemporaryDir dir;
        CustomThemes store(dir.path());
        QVERIFY(store.beginNew(QStringLiteral("default")));
        const QString id = store.saveDraft();
        QCOMPARE(jsonFiles(dir.path()), 1);
        QVERIFY(store.beginEdit(id));
        QSignalSpy changed(&store, &CustomThemes::changed);
        QVERIFY(store.remove(id));
        QCOMPARE(changed.count(), 1);
        QVERIFY(!store.editing());
        QCOMPARE(jsonFiles(dir.path()), 0);
        QVERIFY(!store.find(id));
        QVERIFY(!store.remove(id));
    }

    void removeDeletesAllFilesWithTheSameId()
    {
        QTemporaryDir dir;
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Alpha"), kGoodId),
                          R"({"schema":1,"name":"Alpha"})"));
        QVERIFY(writeFile(storedThemePath(dir.path(), QStringLiteral("Zulu"), kGoodId),
                          R"({"schema":1,"name":"Zulu"})"));
        CustomThemes store(dir.path());
        QCOMPARE(store.families().size(), 1);

        QVERIFY(store.remove(kGoodId));

        QVERIFY(!store.find(kGoodId));
        QCOMPARE(jsonFiles(dir.path()), 0);
    }

    // --- Theme, with a settings file of its own ---------------------------

    void themeKeepsItsThemesBesideTheSettingsFile()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("swiftify.conf")));
        Theme theme(&settings);
        QVERIFY(theme.customThemes()->beginNew(QStringLiteral("default")));
        const QString id = theme.customThemes()->saveDraft();
        QVERIFY(!id.isEmpty());
        QVERIFY(QFileInfo::exists(storedThemePath(dir.filePath(QStringLiteral("themes")),
                                                  QStringLiteral("My theme"), id)));
        QCOMPARE(theme.customThemes()->parent(), &theme);
    }

    void aSavedThemeIsTheThemeAfterARestart()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("swiftify.conf"));
        QString id;
        {
            Settings settings(path);
            settings.setThemeMode(QStringLiteral("dark"));
            Theme theme(&settings);
            CustomThemes *store = theme.customThemes();
            QVERIFY(store->beginNew(QStringLiteral("nord")));
            QVERIFY(store->setDraftColor(true, QStringLiteral("primary"), QColor(0x11, 0x22, 0x33)));
            id = store->saveDraft();
            settings.setThemeSource(id);
            QTRY_COMPARE(theme.colors()->primary(), QColor(0x11, 0x22, 0x33));
        }
        Settings settings(path);
        Theme theme(&settings);
        QCOMPARE(settings.themeSource(), id);
        QVERIFY(!theme.usingSystemScheme());
        QCOMPARE(theme.colors()->primary(), QColor(0x11, 0x22, 0x33));
        QVERIFY(!optionName(theme, id).isEmpty());
    }

    void aDraftRecoloursTheAppUntilItIsDiscarded()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeMode(QStringLiteral("dark"));
        settings.setThemeSource(QStringLiteral("spotify"));
        Theme theme(&settings);
        const QColor before = theme.colors()->primary();
        CustomThemes *store = theme.customThemes();
        QVERIFY(store->beginNew(QStringLiteral("spotify")));
        QVERIFY(store->setDraftColor(true, QStringLiteral("primary"), QColor(1, 2, 3)));
        QTRY_COMPARE(theme.colors()->primary(), QColor(1, 2, 3));
        QVERIFY(QColor(1, 2, 3) != before);
        store->discardDraft();
        QTRY_COMPARE(theme.colors()->primary(), before);
        QCOMPARE(settings.themeSource(), QStringLiteral("spotify"));
        QCOMPARE(store->families().size(), 0);
    }

    void aDraftFollowsLightAndDark()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeMode(QStringLiteral("dark"));
        Theme theme(&settings);
        CustomThemes *store = theme.customThemes();
        QVERIFY(store->beginNew(QStringLiteral("default")));
        QVERIFY(store->setDraftColor(true, QStringLiteral("primary"), QColor(1, 2, 3)));
        QVERIFY(store->setDraftColor(false, QStringLiteral("primary"), QColor(4, 5, 6)));
        QTRY_COMPARE(theme.colors()->primary(), QColor(1, 2, 3));
        QVERIFY(theme.dark());
        settings.setThemeMode(QStringLiteral("light"));
        QTRY_COMPARE(theme.colors()->primary(), QColor(4, 5, 6));
        QVERIFY(!theme.dark());
        settings.setThemeMode(QStringLiteral("dark"));
        QTRY_COMPARE(theme.colors()->primary(), QColor(1, 2, 3));
        QVERIFY(theme.dark());
    }

    void theOptionsFollowSaveRenameAndDelete()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("swiftify.conf")));
        Theme theme(&settings);
        CustomThemes *store = theme.customThemes();
        QVERIFY(store->beginNew(QStringLiteral("default")));
        store->setDraftName(QStringLiteral("First"));
        const QString id = store->saveDraft();
        QCOMPARE(optionName(theme, id), QStringLiteral("First"));
        QVERIFY(store->beginEdit(id));
        store->setDraftName(QStringLiteral("Second"));
        QCOMPARE(store->saveDraft(), id);
        QCOMPARE(optionName(theme, id), QStringLiteral("Second"));
        QVERIFY(store->remove(id));
        QVERIFY(optionName(theme, id).isEmpty());
        // the built-in ones are still all there
        QVERIFY(!optionName(theme, QStringLiteral("nord")).isEmpty());
        QVERIFY(!optionName(theme, QStringLiteral("builtin")).isEmpty());
    }

    void deletingTheActiveThemeFallsBack()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("swiftify.conf")));
        settings.setThemeMode(QStringLiteral("dark"));
        Theme theme(&settings);
        CustomThemes *store = theme.customThemes();
        QVERIFY(store->beginNew(QStringLiteral("default")));
        QVERIFY(store->setDraftColor(true, QStringLiteral("primary"), QColor(1, 2, 3)));
        const QString id = store->saveDraft();
        settings.setThemeSource(id);
        QTRY_COMPARE(theme.colors()->primary(), QColor(1, 2, 3));
        QVERIFY(store->remove(id));
        QTRY_COMPARE(theme.colors()->roles(), Theme::rolesFromSeed(QColor::fromRgba(Theme::kAccent), true, true));
    }

    void aBrokenThemeFileDoesNotStopTheApp()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("swiftify.conf"));
        {
            Settings settings(path);
            settings.setThemeSource(kGoodId);
        }
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("themes"))));
        QVERIFY(writeFile(storedThemePath(dir.filePath(QStringLiteral("themes")), QStringLiteral("Broken"), kGoodId),
                          "{ broken"));
        Settings settings(path);
        Theme theme(&settings);
        QCOMPARE(settings.themeSource(), kGoodId);
        QVERIFY(theme.colors()->primary().isValid());
        QCOMPARE(theme.colors()->roles(),
                 Theme::rolesFromSeed(QColor::fromRgba(Theme::kAccent), theme.dark(), true));
    }
};

QTEST_MAIN(CustomThemesTest)
#include "tst_customthemes.moc"