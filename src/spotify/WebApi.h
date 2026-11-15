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

#include "spotify/SpotifyTypes.h"

#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QCoroTask>

#include <functional>

class QNetworkAccessManager;

namespace swiftify {

/// the spotify web api calls the library needs, as SpotifyWebAPI.swift makes
/// them. every call throws SpotifyError on failure; a 401 surfaces as Unauthorized
/// so the caller can refresh its token and retry once.
class WebApi {
public:
    static constexpr int kMaximumCoverBytes = 256'000;

    explicit WebApi(QNetworkAccessManager *network);

    QCoro::Task<Profile> profile(QString accessToken);
    QCoro::Task<QList<Playlist>> playlists(QString accessToken);
    QCoro::Task<QList<Song>> playlistSongs(QString playlistId, QString accessToken);
    QCoro::Task<QList<Song>> savedSongs(QString accessToken);
    /// how many songs are saved, from one page of one: the count without the list
    QCoro::Task<int> savedSongTotal(QString accessToken);
    QCoro::Task<QList<Song>> recentlyPlayed(QString accessToken);
    QCoro::Task<SearchResults> search(QString query, QString accessToken);
    QCoro::Task<Song> song(QString trackId, QString accessToken);
    QCoro::Task<Album> album(QString albumId, QString accessToken);
    QCoro::Task<Playlist> playlist(QString playlistId, QString accessToken);
    QCoro::Task<Artist> artist(QString artistId, QString accessToken);
    QCoro::Task<QList<Song>> albumSongs(Album album, QString accessToken);
    QCoro::Task<QList<Album>> artistAlbums(Artist artist, QString accessToken);
    /// the genres spotify gives each artist, by id, and their pictures; an
    /// artist it gives none for is still there, with an empty list
    QCoro::Task<QHash<QString, QStringList>> artistGenres(QStringList artistIds, QString accessToken,
                                                         QHash<QString, QUrl> *artwork = nullptr);

    QCoro::Task<Playlist> createPlaylist(QString name, QString description, bool isPublic, QString accessToken);
    QCoro::Task<> updatePlaylist(QString id, QString name, QString description, bool isPublic, QString accessToken);
    QCoro::Task<> addToPlaylist(QString songUri, QString playlistId, QString accessToken);
    QCoro::Task<> removeFromPlaylist(QString songUri, QString playlistId, QString accessToken);
    /// moves a range, in spotify's indexes
    QCoro::Task<> reorderPlaylist(QString playlistId, int rangeStart, int insertBefore, int length,
                                  QString accessToken);
    /// a jpeg of at most kMaximumCoverBytes. needs the ugc-image-upload scope.
    QCoro::Task<> setPlaylistImage(QString playlistId, QByteArray jpeg, QString accessToken);
    /// spotify has no delete. removing a playlist from a library is an unfollow.
    QCoro::Task<> deletePlaylist(QString id, QString accessToken);
    /// saves or removes any spotify uri in the library
    QCoro::Task<> setSaved(bool saved, QString uri, QString accessToken);
    /// at most 40 uris are checked
    QCoro::Task<QHash<QString, bool>> savedStatuses(QStringList uris, QString accessToken);

    QCoro::Task<QList<ConnectDevice>> devices(QString accessToken);
    QCoro::Task<RemotePlaybackState> playbackState(QString accessToken);
    QCoro::Task<> transferPlayback(QString deviceId, bool play, QString accessToken);
    QCoro::Task<> remotePlay(QString deviceId, QStringList uris, int offsetPosition, int positionMs, QString accessToken);
    QCoro::Task<> remotePause(QString deviceId, QString accessToken);
    QCoro::Task<> remoteNext(QString deviceId, QString accessToken);
    QCoro::Task<> remotePrevious(QString deviceId, QString accessToken);
    QCoro::Task<> remoteSeek(int positionMs, QString deviceId, QString accessToken);
    QCoro::Task<> remoteSetVolume(int volumePercent, QString deviceId, QString accessToken);
    QCoro::Task<> remoteSetShuffle(bool state, QString deviceId, QString accessToken);
    QCoro::Task<> remoteSetRepeat(QString state, QString deviceId, QString accessToken);

    static QUrl apiUrl(const QString &path, const QUrlQuery &query = {});

private:
    QCoro::Task<QByteArray> requestData(QUrl url, QByteArray method, QString accessToken,
                                        QByteArray body = {}, QByteArray contentType = {});
    QCoro::Task<QJsonDocument> requestJson(QUrl url, QByteArray method, QString accessToken,
                                           QByteArray body = {}, QByteArray contentType = {});
    /// follows `next` until it runs out. a page url seen twice is an error,
    /// rather than a loop.
    QCoro::Task<> collectPages(QUrl firstUrl, QString accessToken,
                               std::function<void(const QJsonObject &page)> onPage);
    /// every page of a list that pages by offset, the first alone for its
    /// total and the rest a few at a time, handed to `onPage` in order. a list
    /// of a thousand songs comes in the time of five pages, not twenty.
    QCoro::Task<> collectOffsetPages(QString path, QUrlQuery query, QString accessToken,
                                     std::function<void(const QJsonObject &page)> onPage);
    /// one page, waited for and asked again once if spotify says to slow down
    QCoro::Task<QJsonObject> requestPage(QUrl url, QString accessToken);

    QNetworkAccessManager *m_network;
};

} // namespace swiftify
