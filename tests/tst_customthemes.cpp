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
        QVERIFY(writeFile(dir.filePath(kGoodId + QStringLiteral(".json")), good));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-not-a-uuid.json")), good));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174000.backup.json")), good));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-123E4567-E89B-42D3-A456-426614174009.json")), good));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174001.json")), "{ nope"));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174002.json")),
                          QByteArray(300 * 1024, ' ') + good));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("custom-123e4567-e89b-42d3-a456-426614174003.json")),
                          R"({"schema":2,"name":"Future"})"));
        CustomThemes store(dir.path());
        QCOMPARE(store.families().size(), 1);
        QCOMPARE(store.families().first().id, kGoodId);
    }

    void rejectsBadImports_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("no schema") << QByteArray(R"({"name":"X"})");
        QTest::newRow("future schema") << QByteArray(R"({"schema":2,"name":"X"})");
        QTest::newRow("no name") << QByteArray(R"({"schema":1})");
        QTest::newRow("not json") << QByteArray("{ nope");
        QTest::newRow("not an object") << QByteArray("[1, 2]");
        QTest::newRow("colour name") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":"red"}})");
        QTest::newRow("short hex") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":"#abc"}})");
        QTest::newRow("no hash") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":"aabbcc"}})");
        QTest::newRow("alpha hex") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":"#aabbccdd"}})");
        QTest::newRow("colour not a string") << QByteArray(R"({"schema":1,"name":"X","dark":{"primary":123}})");
        QTest::newRow("variant not an object") << QByteArray(R"({"schema":1,"name":"X","dark":[]})");
    }

    void rejectsBadImports()
    {
        QFETCH(QByteArray, json);
        QTemporaryDir dir;
        QTemporaryDir source;
        const QString file = source.filePath(QStringLiteral("t.json"));
        QVERIFY(writeFile(file, json));
        CustomThemes store(dir.path());
        QSignalSpy errors(&store, &CustomThemes::errorChanged);
        QVERIFY(store.importTheme(file).isEmpty());
        QVERIFY(!store.lastError().isEmpty());
        QCOMPARE(errors.count(), 1);
        QCOMPARE(store.families().size(), 0);
        QCOMPARE(jsonFiles(dir.path()), 0);
    }

    void importRefusesMissingAndOversizedFiles()
    {
        QTemporaryDir dir;
        QTemporaryDir source;
        CustomThemes store(dir.path());
        QVERIFY(store.importTheme(source.filePath(QStringLiteral("nothing.json"))).isEmpty());
        QVERIFY(store.importTheme(QString()).isEmpty());
        const QString big = source.filePath(QStringLiteral("big.json"));
        QVERIFY(writeFile(big, QByteArray(300 * 1024, ' ') + R"({"schema":1,"name":"X"})"));
        QVERIFY(store.importTheme(big).isEmpty());
        QCOMPARE(store.families().size(), 0);
    }

    void importFillsGapsFromBaseAndNeverOverwrites()
    {
        QTemporaryDir dir;
        QTemporaryDir source;
        const QString file = source.filePath(QStringLiteral("t.json"));
        QVERIFY(writeFile(file, R"({"schema":1,"name":"X","base":"nord","dark":{"primary":"#010203"},"unknown":1})"));
        CustomThemes store(dir.path());
        const QString first = store.importTheme(file);
        const QString second = store.importTheme(file);
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        QVERIFY(first != second);
        QCOMPARE(store.families().size(), 2);
        QCOMPARE(jsonFiles(dir.path()), 2);
        const themes::Family *family = store.find(first);
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

    void exportNeverOverwritesAndRoundTrips()
    {
        QTemporaryDir dir;
        QTemporaryDir out;
        CustomThemes store(dir.path());
        QVERIFY(store.beginNew(QStringLiteral("dracula")));
        store.setDraftName(QStringLiteral("My / Theme"));
        const QString id = store.saveDraft();
        QVERIFY(store.exportTheme(id, out.path()));
        const QStringList files = QDir(out.path()).entryList({QStringLiteral("*.json")}, QDir::Files);
        QCOMPARE(files.size(), 1);
        QVERIFY(!files.first().contains(u'/'));

        // a second export to the same place is refused, and the file is left as it was
        const QString exported = out.filePath(files.first());
        QFile before(exported);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const QByteArray content = before.readAll();
        before.close();
        QVERIFY(!store.exportTheme(id, out.path()));
        QVERIFY(store.lastError().contains(QStringLiteral("already exists")));
        QVERIFY(!store.exportTheme(id, exported));
        QFile after(exported);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), content);

        const QString imported = store.importTheme(exported);
        QVERIFY(!imported.isEmpty());
        QVERIFY(imported != id);
        const themes::Family *a = store.find(id);
        const themes::Family *b = store.find(imported);
        QCOMPARE(a->name, b->name);
        QCOMPARE(a->light.primary, b->light.primary);
        QCOMPARE(a->dark.window, b->dark.window);
        QVERIFY(!store.exportTheme(QStringLiteral("custom-nope"), out.path()));
        QVERIFY(!store.exportTheme(id, QString()));
        // a name without a suffix gets one
        QVERIFY(store.exportTheme(id, out.filePath(QStringLiteral("plain"))));
        QVERIFY(QFileInfo::exists(out.filePath(QStringLiteral("plain.json"))));
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
        QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("themes/") + id + QStringLiteral(".json"))));
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
        QVERIFY(writeFile(dir.filePath(QStringLiteral("themes/") + kGoodId + QStringLiteral(".json")), "{ broken"));
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