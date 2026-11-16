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

#include "theme/Themes.h"

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// the user's own themes, one json file each, and the one being edited. owned by Theme
class CustomThemes : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Custom themes come from Theme.")

    /// the list changed
    Q_PROPERTY(int revision READ revision NOTIFY changed FINAL)
    Q_PROPERTY(bool editing READ editing NOTIFY draftChanged FINAL)
    Q_PROPERTY(bool dirty READ dirty NOTIFY draftChanged FINAL)
    Q_PROPERTY(int draftVersion READ draftVersion NOTIFY draftChanged FINAL)
    Q_PROPERTY(QString draftId READ draftId NOTIFY draftChanged FINAL)
    Q_PROPERTY(QString draftName READ draftName WRITE setDraftName NOTIFY draftChanged FINAL)
    /// why the last call that returned false or an empty id failed
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged FINAL)

public:
    static constexpr int kSchema = 1;

    /// `directory` holds one json file per theme
    explicit CustomThemes(const QString &directory, QObject *parent = nullptr);

    const QList<themes::Family> &families() const { return m_families; }
    /// nullptr unless id is a custom theme
    const themes::Family *find(const QString &id) const;
    /// the theme being edited, nullptr when none is
    const themes::Family *draft() const { return m_editing ? &m_draft : nullptr; }

    int revision() const { return m_revision; }
    bool editing() const { return m_editing; }
    bool dirty() const;
    int draftVersion() const { return m_draftVersion; }
    QString draftId() const { return m_draft.id; }
    QString draftName() const { return m_draft.name; }
    void setDraftName(const QString &name);
    QString lastError() const { return m_error; }

    Q_INVOKABLE QVariantList entries() const;
    /// a new theme from any theme's colours; an unknown id takes the default theme's
    Q_INVOKABLE bool beginNew(const QString &templateId);
    Q_INVOKABLE bool beginEdit(const QString &id);
    Q_INVOKABLE QColor draftColor(bool dark, const QString &key) const;
    Q_INVOKABLE bool setDraftColor(bool dark, const QString &key, const QColor &color);
    /// exactly six hex digits, with or without a leading #; anything else sets lastError
    Q_INVOKABLE bool setDraftColorHex(bool dark, const QString &key, const QString &text);
    Q_INVOKABLE void resetDraftColor(bool dark, const QString &key);
    Q_INVOKABLE void resetDraft();
    /// the saved theme's id, empty (see lastError) on failure
    Q_INVOKABLE QString saveDraft();
    Q_INVOKABLE void discardDraft();
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE QString duplicate(const QString &id);
    /// "#rrggbb", lowercase, opaque
    Q_INVOKABLE static QString hex(const QColor &color);
    /// an invalid colour unless text is exactly six hex digits, # optional
    Q_INVOKABLE static QColor parseHex(const QString &text);
    Q_INVOKABLE static qreal contrast(const QColor &a, const QColor &b);

signals:
    void changed();
    void draftChanged();
    void errorChanged();

private:
    const themes::Family *lookup(const QString &id) const;
    void load();
    void sortFamilies();
    bool writeFile(const themes::Family &family);
    bool writeJson(const QString &path, const themes::Family &family);
    QString addFamily(const themes::Family &family);
    void startDraft(bool isNew);
    /// sets the error and returns false
    bool fail(const QString &message);

    QString m_directory;
    QList<themes::Family> m_families;
    themes::Family m_draft {};
    themes::Family m_original {};
    bool m_editing = false;
    bool m_isNew = false;
    int m_revision = 0;
    int m_draftVersion = 0;
    QString m_error;
};

}