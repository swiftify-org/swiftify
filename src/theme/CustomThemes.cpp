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

#include "theme/CustomThemes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <array>
#include <optional>

namespace swiftify {

Q_LOGGING_CATEGORY(lcCustomThemes, "swiftify.theme.custom")

namespace {

struct Field {
    const char *key;
    QRgb themes::Variant::*member;
};

// the json keys are the Variant's own field names
constexpr std::array<Field, 13> kFields {{
    {"lowest", &themes::Variant::lowest},
    {"window", &themes::Variant::window},
    {"page", &themes::Variant::page},
    {"raised", &themes::Variant::raised},
    {"raisedMore", &themes::Variant::raisedMore},
    {"line", &themes::Variant::line},
    {"text", &themes::Variant::text},
    {"subtext", &themes::Variant::subtext},
    {"muted", &themes::Variant::muted},
    {"primary", &themes::Variant::primary},
    {"secondary", &themes::Variant::secondary},
    {"tertiary", &themes::Variant::tertiary},
    {"error", &themes::Variant::error},
}};

constexpr qint64 kMaxFileSize = 256 * 1024;
constexpr int kMaxNameLength = 60;

QString tr(const char *text)
{
    return QCoreApplication::translate("CustomThemes", text);
}

const Field *fieldFor(const QString &key)
{
    for (const Field &field : kFields) {
        if (key == QLatin1StringView(field.key)) {
            return &field;
        }
    }
    return nullptr;
}

QString themeIdFromFileName(const QString &baseName)
{
    static const QRegularExpression pattern(QStringLiteral(
        "^.+--(custom-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$"));

    const auto match = pattern.match(baseName);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString themeFileName(const themes::Family &family)
{
    QString name = family.name.trimmed();

    static const QRegularExpression invalidCharacters(
        QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]"));

    name.replace(invalidCharacters, QStringLiteral("_"));

    // Windows filenames cannot end with a dot or a space.
    while (!name.isEmpty() && (name.endsWith(u'.') || name.endsWith(u' '))) {
        name.chop(1);
    }

    if (name.isEmpty()) {
        name = QStringLiteral("Theme");
    }

    return name + QStringLiteral("--") + family.id + QStringLiteral(".json");
}

QStringList themeFilePathsForId(const QString &directory, const QString &id)
{
    const QDir dir(directory);
    QStringList paths;

    for (const QFileInfo &info : dir.entryInfoList(
             {QStringLiteral("*.json")}, QDir::Files)) {
        if (themeIdFromFileName(info.completeBaseName()) == id) {
            paths.append(info.absoluteFilePath());
        }
    }

    return paths;
}

QString newId()
{
    return QStringLiteral("custom-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool sameVariant(const themes::Variant &a, const themes::Variant &b)
{
    return std::all_of(kFields.begin(), kFields.end(), [&](const Field &field) { return a.*field.member == b.*field.member; });
}

QJsonObject variantToJson(const themes::Variant &variant)
{
    QJsonObject object;
    for (const Field &field : kFields) {
        object.insert(QLatin1StringView(field.key), CustomThemes::hex(QColor::fromRgba(variant.*field.member)));
    }
    return object;
}

/// the keys present are read over `variant`; missing ones keep what it holds.
/// a colour in a file is "#rrggbb", nothing else
bool readVariant(const QJsonValue &value, const QString &which, themes::Variant &variant, QString &error)
{
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isObject()) {
        error = tr("\"%1\" must be an object").arg(which);
        return false;
    }
    const QJsonObject object = value.toObject();
    for (const Field &field : kFields) {
        const QJsonValue entry = object.value(QLatin1StringView(field.key));
        if (entry.isUndefined()) {
            continue;
        }
        const QString text = entry.toString();
        const QColor color = entry.isString() && text.startsWith(u'#') ? CustomThemes::parseHex(text) : QColor();
        if (!color.isValid()) {
            error = tr("%1.%2 is not a colour like #aabbcc").arg(which, QLatin1StringView(field.key));
            return false;
        }
        variant.*field.member = color.rgb() | 0xff000000u;
    }
    return true;
}

std::optional<themes::Family> parse(const QJsonObject &object, const QString &id, QString &error)
{
    const int schema = object.value(QStringLiteral("schema")).toInt(0);
    if (schema < 1) {
        error = tr("This is not a Swiftify theme file");
        return std::nullopt;
    }
    if (schema > CustomThemes::kSchema) {
        error = tr("This theme was made by a newer Swiftify (format %1)").arg(schema);
        return std::nullopt;
    }
    const QString name = object.value(QStringLiteral("name")).toString().trimmed().left(kMaxNameLength);
    if (name.isEmpty()) {
        error = tr("The theme has no name");
        return std::nullopt;
    }
    // what a file leaves out comes from its base theme, else the default one
    const themes::Family *base = themes::find(object.value(QStringLiteral("base")).toString());
    if (!base) {
        base = themes::find(QStringLiteral("default"));
    }
    themes::Family family {id, name, base->light, base->dark};
    if (!readVariant(object.value(QStringLiteral("light")), QStringLiteral("light"), family.light, error)
        || !readVariant(object.value(QStringLiteral("dark")), QStringLiteral("dark"), family.dark, error)) {
        return std::nullopt;
    }
    return family;
}

std::optional<themes::Family> readFile(const QString &path, const QString &id, QString &error)
{
    const QFileInfo info(path);
    if (!info.isFile()) {
        error = tr("The file does not exist");
        return std::nullopt;
    }
    if (info.size() > kMaxFileSize) {
        error = tr("The file is too large for a theme");
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = tr("The file could not be read");
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = tr("The file is not valid JSON");
        return std::nullopt;
    }
    return parse(document.object(), id, error);
}

} // namespace

CustomThemes::CustomThemes(const QString &directory, QObject *parent)
    : QObject(parent)
    , m_directory(directory)
{
    load();
}

void CustomThemes::load()
{
    m_families.clear();
    const QDir directory(m_directory);
    QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable);
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return QString::compare(a.fileName(), b.fileName(), Qt::CaseSensitive) < 0;
    });
    QSet<QString> loadedIds;

    // a theme that will not load is skipped, never fatal: Theme falls back to the app's own colours
    for (const QFileInfo &info : files) {
        const QString id = themeIdFromFileName(info.completeBaseName());
        QString error;

        if (id.isEmpty()) {
            qCWarning(lcCustomThemes) << "skipping" << info.fileName() << ": not a theme file name";
        } else if (loadedIds.contains(id)) {
            qCWarning(lcCustomThemes) << "skipping duplicate theme UUID in" << info.fileName();
        } else if (const auto family = readFile(info.absoluteFilePath(), id, error)) {
            m_families.append(*family);
            loadedIds.insert(id);
        } else {
            qCWarning(lcCustomThemes) << "skipping" << info.fileName() << ":" << error;
        }
    }
    sortFamilies();
}

void CustomThemes::sortFamilies()
{
    std::sort(m_families.begin(), m_families.end(), [](const themes::Family &a, const themes::Family &b) {
        const int order = a.name.localeAwareCompare(b.name);
        return order != 0 ? order < 0 : a.id < b.id;
    });
}

const themes::Family *CustomThemes::find(const QString &id) const
{
    for (const themes::Family &family : m_families) {
        if (family.id == id) {
            return &family;
        }
    }
    return nullptr;
}

const themes::Family *CustomThemes::lookup(const QString &id) const
{
    const themes::Family *family = themes::find(id);
    return family ? family : find(id);
}

bool CustomThemes::fail(const QString &message)
{
    m_error = message;
    emit errorChanged();
    return false;
}

QString CustomThemes::hex(const QColor &color)
{
    return QColor::fromRgb(color.rgb()).name(QColor::HexRgb);
}

QColor CustomThemes::parseHex(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("\\A#?([0-9a-fA-F]{6})\\z"));
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch()) {
        return {};
    }
    bool ok = false;
    const QRgb rgb = match.captured(1).toUInt(&ok, 16);
    return ok ? QColor::fromRgb(rgb) : QColor();
}


bool CustomThemes::writeJson(const QString &path, const themes::Family &family)
{
    QJsonObject object;
    object.insert(QStringLiteral("schema"), kSchema);
    object.insert(QStringLiteral("name"), family.name);
    object.insert(QStringLiteral("light"), variantToJson(family.light));
    object.insert(QStringLiteral("dark"), variantToJson(family.dark));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(tr("Could not write %1").arg(path));
    }

    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);

    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return fail(tr("Could not write %1").arg(path));
    }

    // Atomically replace the destination file.
    if (!file.commit()) {
        return fail(tr("Could not write %1").arg(path));
    }

    return true;
}


bool CustomThemes::writeFile(const themes::Family &family)
{
    if (!QDir().mkpath(m_directory)) {
        return fail(tr("Could not create the themes folder"));
    }

    const QDir directory(m_directory);
    const QString targetPath = directory.absoluteFilePath(themeFileName(family));

    // Keep an existing theme at its original path. Changing the JSON name is
    // atomic with QSaveFile; moving to a name-derived path would require a
    // second, non-atomic deletion that can fail after a successful write.
    if (find(family.id)) {
        QStringList paths = themeFilePathsForId(m_directory, family.id);
        std::sort(paths.begin(), paths.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseSensitive) < 0;
        });
        for (const QString &path : paths) {
            if (!QFileInfo(path).isReadable()) {
                continue;
            }
            QString error;
            if (readFile(path, family.id, error)) {
                return writeJson(path, family);
            }
        }
        return fail(tr("Could not write %1").arg(targetPath));
    }

    return writeJson(targetPath, family);
}

QString CustomThemes::addFamily(const themes::Family &family)
{
    m_families.append(family);
    sortFamilies();
    ++m_revision;
    emit changed();
    return family.id;
}

QVariantList CustomThemes::entries() const
{
    QVariantList list;
    for (const themes::Family &family : m_families) {
        list.append(QVariantMap {{QStringLiteral("id"), family.id}, {QStringLiteral("name"), family.name}});
    }
    return list;
}

bool CustomThemes::dirty() const
{
    return m_editing && (m_isNew || m_draft.name != m_original.name || !sameVariant(m_draft.light, m_original.light)
                         || !sameVariant(m_draft.dark, m_original.dark));
}

void CustomThemes::startDraft(bool isNew)
{
    m_original = m_draft;
    m_editing = true;
    m_isNew = isNew;
    ++m_draftVersion;
    emit draftChanged();
}

bool CustomThemes::beginNew(const QString &templateId)
{
    const themes::Family *base = lookup(templateId);
    if (!base) {
        base = themes::find(QStringLiteral("default"));
    }
    m_draft = *base;
    m_draft.id = newId();
    m_draft.name = tr("My theme");
    startDraft(true);
    return true;
}

bool CustomThemes::beginEdit(const QString &id)
{
    const themes::Family *family = find(id);
    if (!family) {
        return fail(tr("That theme does not exist"));
    }
    m_draft = *family;
    startDraft(false);
    return true;
}

void CustomThemes::setDraftName(const QString &name)
{
    const QString limited = name.left(kMaxNameLength);
    if (!m_editing || m_draft.name == limited) {
        return;
    }
    m_draft.name = limited;
    ++m_draftVersion;
    emit draftChanged();
}

QColor CustomThemes::draftColor(bool dark, const QString &key) const
{
    const Field *field = fieldFor(key);
    return field ? QColor::fromRgba((dark ? m_draft.dark : m_draft.light).*field->member) : QColor();
}

bool CustomThemes::setDraftColor(bool dark, const QString &key, const QColor &color)
{
    const Field *field = fieldFor(key);
    if (!m_editing || !field || !color.isValid()) {
        return false;
    }
    QRgb &slot = (dark ? m_draft.dark : m_draft.light).*field->member;
    const QRgb value = color.rgb() | 0xff000000u;
    if (slot == value) {
        return true;
    }
    slot = value;
    ++m_draftVersion;
    emit draftChanged();
    return true;
}

bool CustomThemes::setDraftColorHex(bool dark, const QString &key, const QString &text)
{
    const QColor color = parseHex(text);
    if (!color.isValid()) {
        return fail(tr("Use six hex digits, like #aabbcc"));
    }
    return setDraftColor(dark, key, color);
}

void CustomThemes::resetDraftColor(bool dark, const QString &key)
{
    if (const Field *field = fieldFor(key); field && m_editing) {
        setDraftColor(dark, key, QColor::fromRgba((dark ? m_original.dark : m_original.light).*field->member));
    }
}

void CustomThemes::resetDraft()
{
    if (!m_editing) {
        return;
    }
    m_draft = m_original;
    ++m_draftVersion;
    emit draftChanged();
}

QString CustomThemes::saveDraft()
{
    if (!m_editing) {
        return {};
    }
    const QString name = m_draft.name.trimmed();
    if (name.isEmpty()) {
        fail(tr("The theme needs a name"));
        return {};
    }
    m_draft.name = name;
    if (!writeFile(m_draft)) {
        return {};
    }
    const QString id = m_draft.id;
    m_families.removeIf([&](const themes::Family &family) { return family.id == id; });
    m_families.append(m_draft);
    sortFamilies();
    m_editing = false;
    ++m_revision;
    ++m_draftVersion;
    emit changed();
    emit draftChanged();
    return id;
}

void CustomThemes::discardDraft()
{
    if (!m_editing) {
        return;
    }
    m_editing = false;
    ++m_draftVersion;
    emit draftChanged();
}

bool CustomThemes::remove(const QString &id)
{
    if (!find(id)) {
        return fail(tr("That theme does not exist"));
    }
    const QStringList filePaths = themeFilePathsForId(m_directory, id);
    if (filePaths.isEmpty()) {
        return fail(tr("Could not delete the theme"));
    }

    for (const QString &filePath : filePaths) {
        if (!QFile::remove(filePath)) {
            qCWarning(lcCustomThemes) << "could not delete theme file" << filePath;
        }
    }
    if (!themeFilePathsForId(m_directory, id).isEmpty()) {
        return fail(tr("Could not delete the theme"));
    }

    m_families.removeIf([&](const themes::Family &family) { return family.id == id; });
    ++m_revision;
    const bool wasEditing = m_editing && m_draft.id == id;
    if (wasEditing) {
        m_editing = false;
        ++m_draftVersion;
    }
    emit changed();
    if (wasEditing) {
        emit draftChanged();
    }
    return true;
}

QString CustomThemes::duplicate(const QString &id)
{
    const themes::Family *source = lookup(id);
    if (!source) {
        fail(tr("That theme does not exist"));
        return {};
    }
    themes::Family copy = *source;
    copy.id = newId();
    copy.name = tr("%1 copy").arg(source->name).left(kMaxNameLength);
    return writeFile(copy) ? addFamily(copy) : QString();
}

qreal CustomThemes::contrast(const QColor &a, const QColor &b)
{
    return themes::contrast(a, b);
}

}