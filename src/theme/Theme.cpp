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

#include "theme/Theme.h"

#include "app/Fonts.h"
#include "app/Settings.h"
#include "cpp/cam/hct.h"
#include "cpp/scheme/scheme_content.h"
#include "cpp/scheme/scheme_tonal_spot.h"
#include "theme/ColorMath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QStyleHints>
#include <QStandardPaths>
#include <QUrl>

#include <functional>

namespace swiftify {

Q_LOGGING_CATEGORY(lcTheme, "swiftify.theme")

namespace {

Theme *s_instance = nullptr;

bool systemPrefersLight()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light;
}

QString expandedPath(QString path)
{
    if (QUrl(path).isLocalFile()) path = QUrl(path).toLocalFile();
    if (path == QLatin1StringView("~")) path = QDir::homePath();
    if (path.startsWith(QLatin1StringView("~/"))) path.replace(0, 1, QDir::homePath());
    return QDir::cleanPath(path);
}

QString xdgDirectory(const char *variable, const QString &fallback)
{
    const QString path = qEnvironmentVariable(variable);
    return path.isEmpty() || QDir::isRelativePath(path) ? QDir::homePath() + fallback : QDir::cleanPath(path);
}

QStringList schemeSearchRoots()
{
    const QString config = xdgDirectory("XDG_CONFIG_HOME", QStringLiteral("/.config"));
    const QString cache = xdgDirectory("XDG_CACHE_HOME", QStringLiteral("/.cache"));
    const QString state = xdgDirectory("XDG_STATE_HOME", QStringLiteral("/.local/state"));
    QStringList roots;
#ifndef Q_OS_MACOS
    // Keep end4's existing desktop scheme first on Linux.
    roots.append(state + QStringLiteral("/quickshell/user/generated"));
#endif
    roots.append(config + QStringLiteral("/matugen"));
    roots.append(cache + QStringLiteral("/matugen"));
    roots.append(state + QStringLiteral("/matugen"));
#ifdef Q_OS_MACOS
    for (const QString &base : {QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
                               QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)}) {
        for (const QString &name : {QStringLiteral("com.InioX.matugen"), QStringLiteral("matugen"), QStringLiteral("Matugen")}) {
            roots.append(base + u'/' + name);
        }
    }
    roots.append(state + QStringLiteral("/quickshell/user/generated"));
#endif
    // Also find shell-generated output when a GUI launch has different XDG variables.
    for (const QString &base : {QStringLiteral("/.config"), QStringLiteral("/.cache"), QStringLiteral("/.local/state")}) {
        roots.append(QDir::homePath() + base + QStringLiteral("/matugen"));
    }
    roots.removeDuplicates();
    return roots;
}

/// schemes desktop shells write somewhere of their own, among files that are
/// not schemes, so they are named rather than searched for
QStringList shellSchemeFiles()
{
    const QString cache = xdgDirectory("XDG_CACHE_HOME", QStringLiteral("/.cache"));
    // dankmaterialshell runs matugen into its own cache, beside its clipboard,
    // notifications and image caches
    return {cache + QStringLiteral("/DankMaterialShell/dms-colors.json")};
}

std::optional<MaterialScheme::Roles> readScheme(const QString &path, bool dark)
{
    const QFileInfo info(path);
    // Schemes are small; don't parse an unrelated, huge JSON cache on the GUI thread.
    if (!info.isFile() || info.size() > 4 * 1024 * 1024) return std::nullopt;
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? MaterialScheme::fromMatugenJson(file.readAll(), dark) : std::nullopt;
}

QString existingAncestor(QString path)
{
    while (!QDir(path).exists()) {
        const QString parent = QFileInfo(path).absolutePath();
        if (parent == path) break;
        path = parent;
    }
    return path;
}

QStringList schemeFiles(const QStringList &roots, QStringList &watchedDirectories)
{
    QStringList candidates;
    std::function<void(const QString &, int)> scan = [&](const QString &path, int depth) {
        if (candidates.size() >= 512 || watchedDirectories.size() >= 64) return;
        const QDir directory(path);
        watchedDirectories.append(existingAncestor(path));
        watchedDirectories.removeDuplicates();
        if (!directory.exists()) return;
        const auto files = directory.entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        // Prefer the conventional name; validate every candidate before choosing it.
        const QString colors = directory.filePath(QStringLiteral("colors.json"));
        if (QFileInfo::exists(colors)) candidates.append(colors);
        for (const QFileInfo &file : files) {
            if (candidates.size() >= 512) break;
            if (file.fileName() != QLatin1StringView("colors.json")) candidates.append(file.absoluteFilePath());
        }
        if (depth >= 2) return;
        for (const QFileInfo &child : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name)) {
            const QString name = child.fileName().toLower();
            if (name == QLatin1StringView("templates") || name == QLatin1StringView("template")
                || name == QLatin1StringView("images") || name.startsWith(u'.')) continue;
            scan(child.absoluteFilePath(), depth + 1);
        }
    };
    for (const QString &root : roots) scan(root, 0);
    candidates.removeDuplicates();
    return candidates;
}

} // namespace

Theme::Theme(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_colors(new MaterialScheme(this))
    , m_custom(new CustomThemes(QFileInfo(settings->filePath()).absolutePath() + QStringLiteral("/themes"), this))
{
    if (!s_instance) {
        s_instance = this;
    }

    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(120);
    connect(&m_reloadDebounce, &QTimer::timeout, this, &Theme::reload);
    const auto scheduleReload = [this] { m_reloadDebounce.start(); };
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, scheduleReload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, scheduleReload);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, scheduleReload);
    connect(m_settings, &Settings::themeSourceChanged, this, scheduleReload);
    connect(m_settings, &Settings::themeModeChanged, this, scheduleReload);
    connect(m_settings, &Settings::matugenColorsPathChanged, this, scheduleReload);
    connect(m_custom, &CustomThemes::changed, this, scheduleReload);
    // a theme being edited: every colour change recolours the app, which
    // PlayerStyle follows on the gui thread, so a drag is applied once a frame
    m_draftTimer.setSingleShot(true);
    m_draftTimer.setInterval(16);
    connect(&m_draftTimer, &QTimer::timeout, this, &Theme::reload);
    connect(m_custom, &CustomThemes::draftChanged, this, [this] {
        if (!m_draftTimer.isActive()) {
            m_draftTimer.start();
            }
    });

    reload();
}

Theme::~Theme()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

Theme *Theme::instance()
{
    return s_instance;
}

Theme *Theme::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Theme::resolveSystemScheme(bool dark)
{
    m_schemeDirectories.clear();
    m_schemeFiles.clear();
    m_systemRoles.reset();
    const QString configured = m_settings->matugenColorsPath();
    if (!configured.isEmpty()) {
        m_systemSchemePath = QFileInfo(expandedPath(configured)).absoluteFilePath();
        m_systemRoles = readScheme(m_systemSchemePath, dark);
        m_schemeDirectories.append(existingAncestor(QFileInfo(m_systemSchemePath).absolutePath()));
        return;
    }
    m_systemSchemePath.clear();
    QStringList candidates = schemeFiles(schemeSearchRoots(), m_schemeDirectories);
    // after end4's generated scheme, which is searched first, and before
    // matugen's own folders
    qsizetype shellIndex = 0;
    while (shellIndex < candidates.size() && candidates.at(shellIndex).contains(QLatin1StringView("/quickshell/user/generated/"))) {
        ++shellIndex;
    }
    for (const QString &path : shellSchemeFiles()) {
        if (QFileInfo::exists(path)) {
            candidates.insert(shellIndex++, path);
            m_schemeFiles.append(path);
        } else {
            // until it appears; dms keeps clipboard and notification files
            // beside it, so its folder is watched only while it is missing
            m_schemeDirectories.append(existingAncestor(QFileInfo(path).absolutePath()));
        }
    }
    m_schemeDirectories.removeDuplicates();
    for (const QString &path : candidates) {
        if (const auto roles = readScheme(path, dark)) {
            m_systemSchemePath = path;
            m_systemRoles = roles;
            return;
        }
    }
}

void Theme::refreshSystemScheme()
{
    reload();
}

QString Theme::fontFamily() const
{
    return fonts::interfaceFamily();
}

QString Theme::monoFontFamily() const
{
    return fonts::fixedFamily();
}

MaterialScheme::Roles Theme::rolesFromSeed(QColor seed, bool dark, bool fidelity)
{
    using namespace material_color_utilities;
    const Hct hct(seed.rgba() | 0xff000000u);
    if (fidelity) {
        return MaterialScheme::fromDynamicScheme(SchemeContent(hct, dark, 0.0));
    }
    return MaterialScheme::fromDynamicScheme(SchemeTonalSpot(hct, dark, 0.0));
}

QColor Theme::mix(const QColor &a, const QColor &b, qreal amountOfA)
{
    return color::mix(a, b, amountOfA);
}

QColor Theme::withAlpha(const QColor &color, qreal alpha)
{
    return color::withAlpha(color, alpha);
}

void Theme::reload()
{
    // a theme being edited is the whole app's colours until it is saved or
    // dropped; the desktop's scheme is not read meanwhile
    if (const themes::Family *draft = m_custom->draft()) {
        const bool dark = wantsDark();
        m_usingSystemScheme = false;
        m_colors->setRoles(themes::roles(dark ? draft->dark : draft->light, dark), dark);
        emit schemeChanged();
        return;
    }
    const bool wasUsingSystem = m_usingSystemScheme;
    const bool hadDark = m_colors->dark();
    const QString previousPath = m_systemSchemePath;
    const auto previousRoles = m_colors->roles();

    resolveSystemScheme(wantsDark());
    watchSchemeFile();

    if (m_settings->themeSource() == QLatin1StringView("system") && m_systemRoles) {
        const QColor background = (*m_systemRoles)[static_cast<std::size_t>(MaterialScheme::Role::Background)];
        m_usingSystemScheme = true;
        m_colors->setRoles(*m_systemRoles, color::luminance709(background) < 0.5);
        if (!wasUsingSystem || hadDark != m_colors->dark() || previousPath != m_systemSchemePath || previousRoles != m_colors->roles()) {
            emit schemeChanged();
        }
        return;
    }

    const bool dark = wantsDark();
    m_usingSystemScheme = false;
    const themes::Family *family = themes::find(m_settings->themeSource());
    if (!family) {
        // a custom theme that is gone or will not load falls through to the app's own colours
        family = m_custom->find(m_settings->themeSource());
    }
    if (family) {
        m_colors->setRoles(themes::roles(dark ? family->dark : family->light, dark), dark);
    } else {
        m_colors->setRoles(rolesFromSeed(QColor::fromRgba(kAccent), dark, true), dark);
    }
    if (wasUsingSystem || hadDark != dark || previousPath != m_systemSchemePath || previousRoles != m_colors->roles()) {
        emit schemeChanged();
    }
}

bool Theme::wantsDark() const
{
    const QString mode = m_settings->themeMode();
    if (mode == QLatin1StringView("light")) {
        return false;
    }
    if (mode == QLatin1StringView("dark")) {
        return true;
    }
    return !systemPrefersLight();
}

QList<ThemeOption> Theme::themeOptions(bool dark) const
{
    using Role = MaterialScheme::Role;
    const auto option = [](const QString &id, const QString &name, const MaterialScheme::Roles &roles) {
        const auto role = [&roles](Role which) { return roles[static_cast<std::size_t>(which)]; };
        return ThemeOption {id, name, role(Role::Background), role(Role::SurfaceContainerLow), role(Role::OnSurface),
                            role(Role::Primary), role(Role::Secondary), role(Role::Tertiary)};
    };
    QList<ThemeOption> options;
    // the desktop's own scheme shows as it is, light or dark; before it
    // exists, as the colours it would fall back to
    const auto desktop = readScheme(systemSchemePath(), dark);
    options.append(option(QStringLiteral("system"),
#ifdef Q_OS_MACOS
                          QStringLiteral("Matugen"),
#else
                          QStringLiteral("Desktop"),
#endif
                          desktop ? *desktop : rolesFromSeed(QColor::fromRgba(kAccent), dark, true)));
    options.append(option(QStringLiteral("builtin"), QStringLiteral("Swiftify"), rolesFromSeed(QColor::fromRgba(kAccent), dark, true)));
    for (const themes::Family &family : themes::families()) {
        options.append(option(family.id, family.name, themes::roles(dark ? family.dark : family.light, dark)));
    }
    for (const themes::Family &family : m_custom->families()) {
        options.append(option(family.id, family.name, themes::roles(dark ? family.dark : family.light, dark)));
        }
    return options;
}

void Theme::watchSchemeFile()
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    if (m_settings->themeSource() != QLatin1StringView("system")) {
        return;
    }
    // All discovery directories are watched, including ancestors of missing
    // ones, so a newly generated file can supersede a lower-priority scheme.
    m_watcher.addPaths(m_schemeDirectories);
    if (!m_schemeFiles.isEmpty()) {
        m_watcher.addPaths(m_schemeFiles);
    }
    if (!m_systemSchemePath.isEmpty()) {
        const QFileInfo info(m_systemSchemePath);
        if (info.exists()) {
            m_watcher.addPath(info.absoluteFilePath());
            if (info.canonicalFilePath() != info.absoluteFilePath()) m_watcher.addPath(info.canonicalFilePath());
        }
    }
}

} // namespace swiftify
