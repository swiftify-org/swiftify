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

#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

struct Artist {
    Q_GADGET
    QML_VALUE_TYPE(artist)
    Q_PROPERTY(QString id MEMBER id FINAL)
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QUrl artworkUrl MEMBER artworkUrl FINAL)
    Q_PROPERTY(QString uri MEMBER uri FINAL)
    Q_PROPERTY(bool valid READ isValid FINAL)

public:
    QString id;
    QString name;
    QUrl artworkUrl;
    QString uri;

    bool isValid() const { return !id.isEmpty(); }
    bool operator==(const Artist &) const = default;

    QJsonObject toJson() const;
    static Artist fromJson(const QJsonObject &object);
};

struct Album {
    Q_GADGET
    QML_VALUE_TYPE(album)
    Q_PROPERTY(QString id MEMBER id FINAL)
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QString artists MEMBER artists FINAL)
    Q_PROPERTY(QUrl artworkUrl MEMBER artworkUrl FINAL)
    Q_PROPERTY(QString releaseDate MEMBER releaseDate FINAL)
    Q_PROPERTY(int songCount MEMBER songCount FINAL)
    Q_PROPERTY(QString uri MEMBER uri FINAL)
    Q_PROPERTY(bool valid READ isValid FINAL)

public:
    QString id;
    QString name;
    QString artists;
    QUrl artworkUrl;
    QString releaseDate;
    int songCount = 0;
    QString uri;

    bool isValid() const { return !id.isEmpty(); }
    bool operator==(const Album &) const = default;
};

/// one occurrence of a track. two songs with the same uri are still different
/// songs to the queue, the way the swift app gives each a fresh uuid.
struct Song {
    Q_GADGET
    QML_VALUE_TYPE(song)
    Q_PROPERTY(quint64 instanceId MEMBER instanceId FINAL)
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QString artists MEMBER artists FINAL)
    Q_PROPERTY(QString albumName MEMBER albumName FINAL)
    Q_PROPERTY(QUrl artworkUrl MEMBER artworkUrl FINAL)
    Q_PROPERTY(QString uri MEMBER uri FINAL)
    Q_PROPERTY(int durationMs MEMBER durationMs FINAL)
    Q_PROPERTY(bool isExplicit MEMBER isExplicit FINAL)
    Q_PROPERTY(QString albumId MEMBER albumId FINAL)
    Q_PROPERTY(QString albumUri MEMBER albumUri FINAL)
    Q_PROPERTY(bool isPlayable MEMBER isPlayable FINAL)
    Q_PROPERTY(bool valid READ isValid FINAL)
    /// a music file of the person's own (LocalLibrary), not a spotify track:
    /// nothing spotify knows can be done with it
    Q_PROPERTY(bool isLocalFile READ isLocalFile FINAL)
    Q_PROPERTY(QString shareUrl READ shareUrl FINAL)

public:
    Song();

    quint64 instanceId = 0;
    QString name;
    QString artists;
    QString albumName;
    QUrl artworkUrl;
    QString uri;
    /// 0 when spotify did not say
    int durationMs = 0;
    bool isExplicit = false;
    QString albumId;
    QString albumUri;
    QList<Artist> artistItems;
    /// false when spotify will not give this account the track. the queue
    /// passes over it
    bool isPlayable = true;

    bool isValid() const { return !uri.isEmpty(); }
    /// its uri is the file's own url, file:///...
    bool isLocalFile() const { return uri.startsWith(QLatin1StringView("file:")); }
    QString localPath() const { return isLocalFile() ? QUrl(uri).toLocalFile() : QString(); }
    /// the same occurrence, not merely the same track
    bool isSameInstance(const Song &other) const { return instanceId == other.instanceId; }
    bool operator==(const Song &other) const { return instanceId == other.instanceId; }

    /// the base62 id at the end of the uri
    QString spotifyId() const;
    /// the open.spotify.com share URL for this track
    QString shareUrl() const;
    /// the album to open from this song, when spotify told us enough
    Album albumDestination() const;
    Artist primaryArtist() const { return artistItems.isEmpty() ? Artist {} : artistItems.first(); }
    /// a copy that counts as a new occurrence
    Song freshInstance() const;

    QJsonObject toJson() const;
    static Song fromJson(const QJsonObject &object);
};

struct Playlist {
    Q_GADGET
    QML_VALUE_TYPE(playlist)
    Q_PROPERTY(QString id MEMBER id FINAL)
    Q_PROPERTY(QString name MEMBER name FINAL)
    Q_PROPERTY(QString description MEMBER description FINAL)
    Q_PROPERTY(int songCount MEMBER songCount FINAL)
    Q_PROPERTY(QUrl artworkUrl MEMBER artworkUrl FINAL)
    Q_PROPERTY(QString ownerId MEMBER ownerId FINAL)
    Q_PROPERTY(QString ownerName MEMBER ownerName FINAL)
    Q_PROPERTY(QString ownerDisplayName READ ownerDisplayName FINAL)
    Q_PROPERTY(bool isPublic MEMBER isPublic FINAL)
    Q_PROPERTY(bool isCollaborative MEMBER isCollaborative FINAL)
    Q_PROPERTY(QString uri MEMBER uri FINAL)
    Q_PROPERTY(bool valid READ isValid FINAL)
    /// made on this computer from files of the person's own (LocalLibrary),
    /// not a spotify playlist
    Q_PROPERTY(bool isLocal READ isLocal FINAL)

public:
    QString id;
    QString name;
    QString description;
    int songCount = 0;
    QUrl artworkUrl;
    QString ownerId;
    QString ownerName;
    bool isPublic = false;
    bool isCollaborative = false;
    QString uri;

    bool isValid() const { return !id.isEmpty(); }
    bool isLocal() const { return id.startsWith(QLatin1StringView("local:")); }
    bool operator==(const Playlist &) const = default;
    QString ownerDisplayName() const { return ownerName.isEmpty() ? QStringLiteral("Spotify") : ownerName; }
    bool canModify(const QString &currentUserId) const
    {
        return isCollaborative || (!currentUserId.isEmpty() && ownerId == currentUserId);
    }
};

struct Profile {
    QString id;
    QString displayName;
    QUrl artworkUrl;

    bool isValid() const { return !id.isEmpty(); }
};

struct SearchResults {
    QList<Song> songs;
    QList<Album> albums;
    QList<Artist> artists;
    QList<Playlist> playlists;

    bool isEmpty() const { return songs.isEmpty() && albums.isEmpty() && artists.isEmpty() && playlists.isEmpty(); }
};

} // namespace swiftify

Q_DECLARE_METATYPE(swiftify::Song)
Q_DECLARE_METATYPE(swiftify::Playlist)
Q_DECLARE_METATYPE(swiftify::Album)
Q_DECLARE_METATYPE(swiftify::Artist)
