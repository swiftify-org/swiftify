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

#include "app/Translations.h"
#include "app/ErrorCenter.h"
#include "app/Settings.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocale>

#include <algorithm>

namespace swiftify {

namespace {
bool s_russianEnabled = kRussianTranslationEnabled;
bool s_germanEnabled = kGermanTranslationEnabled;
} // namespace

bool Translations::isLanguageEnabled(QStringView language)
{
    if (language == QLatin1StringView("ru")) {
        return s_russianEnabled;
    }
    if (language == QLatin1StringView("de")) {
        return s_germanEnabled;
    }
    return true;
}

void Translations::setLanguageEnabled(QStringView language, bool enabled)
{
    if (language == QLatin1StringView("ru")) {
        if (s_russianEnabled == enabled) {
            return;
        }
        s_russianEnabled = enabled;
        if (auto *settings = Settings::instance()) {
            settings->enforceEnabledLanguage();
        }
    } else if (language == QLatin1StringView("de")) {
        if (s_germanEnabled == enabled) {
            return;
        }
        s_germanEnabled = enabled;
        if (auto *settings = Settings::instance()) {
            settings->enforceEnabledLanguage();
        }
    }
}

void Translations::resetLanguageEnabled()
{
    setLanguageEnabled(QStringLiteral("ru"), kRussianTranslationEnabled);
    setLanguageEnabled(QStringLiteral("de"), kGermanTranslationEnabled);
}

QString Translations::disabledLanguageMessage()
{
    return QStringLiteral("this language is currently disabled due to missing language maintainers");
}

QString Translations::disabledLanguageErrorMessage()
{
    return QStringLiteral("this language is currently disabled due to missing language maintainers. your app language has been switched back to english.");
}

void Translations::loadCatalog(const QString &resource, Catalog &catalog)
{
    QFile file(resource);
    if (file.open(QIODevice::ReadOnly)) {
        catalog.entries = QJsonDocument::fromJson(file.readAll()).object();
    }
    // Status/error strings can already contain paths, codes or other details
    // when QML receives them. Match catalog templates without translating metadata.
    const QRegularExpression placeholder(QStringLiteral("%([1-9]\\d*)"));
    for (auto it = catalog.entries.begin(); it != catalog.entries.end(); ++it) {
        if (!it.value().isString() || !it.key().contains(placeholder)) continue;
        QString pattern = QStringLiteral("\\A");
        qsizetype position = 0;
        QList<int> arguments;
        auto matches = placeholder.globalMatch(it.key());
        while (matches.hasNext()) {
            const auto match = matches.next();
            pattern += QRegularExpression::escape(it.key().mid(position, match.capturedStart() - position));
            pattern += QStringLiteral("(.*?)");
            arguments.append(match.captured(1).toInt());
            position = match.capturedEnd();
        }
        pattern += QRegularExpression::escape(it.key().mid(position)) + QStringLiteral("\\z");
        catalog.patterns.append({QRegularExpression(pattern, QRegularExpression::DotMatchesEverythingOption),
                                 arguments, it.value().toString()});
    }
    std::sort(catalog.patterns.begin(), catalog.patterns.end(), [](const auto &a, const auto &b) {
        return a.expression.pattern().size() > b.expression.pattern().size();
    });
}

Translations::Translations(Settings *settings, QObject *parent)
    : QTranslator(parent), m_settings(settings)
{
    loadCatalog(QStringLiteral(":/translations/ru.json"), m_russian);
    loadCatalog(QStringLiteral(":/translations/de.json"), m_german);
    connect(settings, &Settings::languageChanged, this, &Translations::apply);
    connect(settings, &Settings::languageAvailabilityChanged, this, &Translations::apply);
    apply();
}

Translations::~Translations()
{
    QCoreApplication::removeTranslator(this);
    QLocale::setDefault(m_previousLocale);
}

void Translations::apply()
{
    QCoreApplication::removeTranslator(this);
    if (!isLanguageEnabled(m_settings->language())) {
        m_settings->enforceEnabledLanguage();
    }
    if (m_settings->takeRevertedDisabledLanguage()) {
        if (auto *errors = ErrorCenter::instance()) {
            errors->show(disabledLanguageErrorMessage());
        }
    }
    const QString language = m_settings->language();
    const bool followSystem = language == QLatin1StringView("system");
    const QLocale::Language systemLanguage = QLocale::system().language();
    m_active = nullptr;
    if (isLanguageEnabled(QStringLiteral("ru"))
        && (language == QLatin1StringView("ru") || (followSystem && systemLanguage == QLocale::Russian))) {
        m_active = &m_russian;
    } else if (isLanguageEnabled(QStringLiteral("de"))
               && (language == QLatin1StringView("de") || (followSystem && systemLanguage == QLocale::German))) {
        m_active = &m_german;
    }
    QLocale::setDefault(m_active == &m_russian ? QLocale(QLocale::Russian, QLocale::Russia)
                        : m_active == &m_german ? QLocale(QLocale::German, QLocale::Germany)
                        : language == QLatin1StringView("en") ? QLocale(QLocale::English, QLocale::UnitedStates)
                                                              : QLocale::system());
    if (m_active) QCoreApplication::installTranslator(this);
    emit languageChanged();
}

QString Translations::translatedMessage(const QString &source, int depth) const
{
    if (!m_active) return {};
    const auto value = m_active->entries.value(source);
    if (value.isString()) return value.toString();
    if (depth >= 4) return {};
    const QRegularExpression placeholder(QStringLiteral("%([1-9]\\d*)"));
    for (const auto &pattern : m_active->patterns) {
        const auto match = pattern.expression.match(source);
        if (!match.hasMatch()) continue;
        QString result;
        qsizetype position = 0;
        auto placeholders = placeholder.globalMatch(pattern.translation);
        while (placeholders.hasNext()) {
            const auto token = placeholders.next();
            result += pattern.translation.mid(position, token.capturedStart() - position);
            const int index = pattern.arguments.indexOf(token.captured(1).toInt());
            const QString argument = match.captured(index + 1);
            const QString translated = translatedMessage(argument, depth + 1);
            result += translated.isEmpty() ? argument : translated;
            position = token.capturedEnd();
        }
        result += pattern.translation.mid(position);
        return result;
    }
    return {};
}

QString Translations::translate(const char *context, const char *sourceText, const char *, int n) const
{
    if (!m_active || !sourceText) return {};
    const auto value = m_active->entries.value(QString::fromUtf8(sourceText));
    if (value.isArray() && n >= 0) {
        // German has two forms (one, other); Russian has three (one, few, many).
        const int form = m_active == &m_german ? (n == 1 ? 0 : 1)
            : n % 10 == 1 && n % 100 != 11 ? 0
            : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14) ? 1 : 2;
        return value.toArray().at(form).toString();
    }
    if (value.isString()) return value.toString();
    const QByteArray scope(context ? context : "");
    if (scope == "Errors" || scope == "Soloist" || scope == "SpotifySession" || scope == "Stats" || scope == "Discovery") {
        return translatedMessage(QString::fromUtf8(sourceText));
    }
    return {};
}

} // namespace swiftify
