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

#include "app/Fonts.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLoggingCategory>

namespace swiftify::fonts {

Q_LOGGING_CATEGORY(lcFonts, "swiftify.fonts")

namespace {

constexpr auto kIconFamily = "Material Symbols Rounded";
constexpr auto kIconFile = "MaterialSymbolsRounded.ttf";
constexpr auto kInterfaceFamily = "Google Sans Flex";
constexpr auto kInterfaceFile = "GoogleSansFlex.ttf";
constexpr auto kGveretFamily = "Gveret Levin";
constexpr auto kGveretFile = "GveretLevin.ttf";
constexpr auto kFredokaFamily = "Fredoka";
constexpr auto kFredokaFile = "Fredoka.ttf";

/// a font installed beside the app, unless the system has it already
bool loadBundled(const char *family, const char *file)
{
    if (QFontDatabase::families().contains(QLatin1StringView(family))) {
        return true;
    }
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QStringList candidates {
        applicationDirectory + QStringLiteral("/../share/swiftify/fonts/"),
        // inside the macos app bundle
        applicationDirectory + QStringLiteral("/../Resources/fonts/"),
        applicationDirectory + QStringLiteral("/fonts/"),
#ifdef SWIFTIFY_BUILD_FONT_DIRECTORY
        QStringLiteral(SWIFTIFY_BUILD_FONT_DIRECTORY "/"),
#endif
    };
    for (const QString &directory : candidates) {
        const QString path = QDir::cleanPath(directory + QLatin1StringView(file));
        if (QFileInfo::exists(path) && QFontDatabase::addApplicationFont(path) >= 0) {
            return true;
        }
    }
    return false;
}

} // namespace

void loadIconFont()
{
    if (!loadBundled(kIconFamily, kIconFile)) {
        qCWarning(lcFonts) << "Material Symbols Rounded is not installed; icons will show as text";
    }
}

void loadInterfaceFont()
{
    if (!loadBundled(kInterfaceFamily, kInterfaceFile)) {
        qCInfo(lcFonts) << "Google Sans Flex is not installed; text uses the next face along, and titles are not rounded";
    }
}

void loadHebrewFont()
{
    if (!loadBundled(kGveretFamily, kGveretFile)) {
        qCInfo(lcFonts) << "Gveret Levin is not installed; hebrew lyrics use the interface font";
    }
    if (!loadBundled(kFredokaFamily, kFredokaFile)) {
        qCInfo(lcFonts) << "Fredoka is not installed; hebrew text uses the interface font";
    }
}

QString iconFamily()
{
    return QString::fromLatin1(kIconFamily);
}

QString interfaceFamily()
{
    static const QString family = [] {
        const QStringList installed = QFontDatabase::families();
        for (const auto *candidate : {"Google Sans Flex", "Google Sans", "Rubik", "Inter", "Noto Sans"}) {
            if (installed.contains(QLatin1StringView(candidate))) {
                return QString::fromLatin1(candidate);
            }
        }
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    }();
    return family;
}

QString fixedFamily()
{
    static const QString family = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    return family;
}

} // namespace swiftify::fonts
