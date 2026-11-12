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

#include "theme/MaterialScheme.h"
#include "theme/CustomThemes.h"
#include "theme/Themes.h"

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class Settings;

/// the app chrome's colours. with the "system" source it follows the scheme
/// Matugen generates from the wallpaper, live; otherwise, or when no
/// such file exists, or another source is chosen, it builds one from the
/// swiftify accent or from one of the rice themes (Themes.h), light or dark as
/// the theme mode says.
class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(swiftify::MaterialScheme *colors READ colors CONSTANT FINAL)
    Q_PROPERTY(swiftify::CustomThemes *customThemes READ customThemes CONSTANT FINAL)
    Q_PROPERTY(bool dark READ dark NOTIFY schemeChanged FINAL)
    Q_PROPERTY(bool usingSystemScheme READ usingSystemScheme NOTIFY schemeChanged FINAL)
    Q_PROPERTY(QString systemSchemePath READ systemSchemePath NOTIFY schemeChanged FINAL)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT FINAL)
    Q_PROPERTY(QString monoFontFamily READ monoFontFamily CONSTANT FINAL)

public:
    /// swiftify's accent, #ff3d8a (MusicStyle.accent)
    static constexpr QRgb kAccent = 0xffff3d8a;

    explicit Theme(Settings *settings, QObject *parent = nullptr);
    ~Theme() override;

    static Theme *instance();
    static Theme *create(QQmlEngine *, QJSEngine *);

    MaterialScheme *colors() const { return m_colors; }
    CustomThemes *customThemes() const { return m_custom; }
    bool dark() const { return m_colors->dark(); }
    bool usingSystemScheme() const { return m_usingSystemScheme; }
    /// the matugen file in use or looked for
    QString systemSchemePath() const { return m_systemSchemePath; }
    QString fontFamily() const;
    QString monoFontFamily() const;

    /// material 3 roles built from a seed colour, the way the fallback scheme is.
    /// `fidelity` keeps the seed's own chroma (artwork); otherwise tonal spot.
    static MaterialScheme::Roles rolesFromSeed(QColor seed, bool dark, bool fidelity);

    /// every colour source the settings offer, as swatches in light or dark:
    /// Matugen's scheme, the app's own, then the rice themes
    Q_INVOKABLE QList<swiftify::ThemeOption> themeOptions(bool dark) const;
    Q_INVOKABLE void refreshSystemScheme();

    Q_INVOKABLE static QColor mix(const QColor &a, const QColor &b, qreal amountOfA);
    Q_INVOKABLE static QColor withAlpha(const QColor &color, qreal alpha);

signals:
    void schemeChanged();

private:
    void reload();
    void resolveSystemScheme(bool dark);
    void watchSchemeFile();
    /// light or dark, from the theme mode and the desktop's preference
    bool wantsDark() const;

    Settings *m_settings;
    MaterialScheme *m_colors;
    CustomThemes *m_custom;
    QFileSystemWatcher m_watcher;
    /// matugen replaces the file, which fires several events in a row
    QTimer m_reloadDebounce;
    /// a colour being dragged is applied at most once a frame
    QTimer m_draftTimer;
    bool m_usingSystemScheme = false;
    QString m_systemSchemePath;
    std::optional<MaterialScheme::Roles> m_systemRoles;
    QStringList m_schemeDirectories;
    /// shells' scheme files that exist, watched themselves rather than their
    /// folders, which the shells fill with other things all the time
    QStringList m_schemeFiles;
};

} // namespace swiftify
