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

#include "spotify/SpotifyJson.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#include <cstdlib>
#include <limits>

namespace swiftify::json {

namespace {

QString string(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1StringView(key)).toString();
}

QString joinedNames(const QJsonArray &artists)
{
    QStringList names;
    for (const QJsonValue &value : artists) {
        names.append(value.toObject().value(QStringLiteral("name")).toString());
    }
    return names.join(QStringLiteral(", "));
}

/// spotify sends a playlist's description as html, its apostrophes as &#x27;
/// and its links as tags
QString plainText(const QString &html)
{
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    static const QRegularExpression entity(QStringLiteral("&(#[0-9]+|#[xX][0-9a-fA-F]+|[a-zA-Z]+);"));
    static const QHash<QString, QString> named {
        {QStringLiteral("amp"), QStringLiteral("&")},   {QStringLiteral("lt"), QStringLiteral("<")},
        {QStringLiteral("gt"), QStringLiteral(">")},    {QStringLiteral("quot"), QStringLiteral("\"")},
        {QStringLiteral("apos"), QStringLiteral("'")},  {QStringLiteral("nbsp"), QStringLiteral(" ")},
    };
    const QString text = QString(html).remove(tag);
    QString result;
    qsizetype from = 0;
    for (auto matches = entity.globalMatch(text); matches.hasNext();) {
        const QRegularExpressionMatch match = matches.next();
        const QString name = match.captured(1);
        QString decoded;
        if (name.startsWith(QLatin1Char('#'))) {
            const bool hex = name.size() > 1 && (name.at(1) == QLatin1Char('x') || name.at(1) == QLatin1Char('X'));
            bool ok = false;
            const uint code = name.mid(hex ? 2 : 1).toUInt(&ok, hex ? 16 : 10);
            if (ok && code > 0 && code <= 0x10FFFF) {
                const char32_t character = code;
                decoded = QString::fromUcs4(&character, 1);
            }
        } else {
            decoded = named.value(name);
        }
        if (decoded.isEmpty()) {
            continue;
        }
        result += QStringView(text).sliced(from, match.capturedStart() - from);
        result += decoded;
        from = match.capturedEnd();
    }
    result += QStringView(text).sliced(from);
    return result;
}

} // namespace

QUrl preferredArtworkUrl(const QJsonArray &images, int preferredWidth)
{
    QUrl best;
    int bestDistance = std::numeric_limits<int>::max();
    for (const QJsonValue &value : images) {
        const QJsonObject image = value.toObject();
        const QUrl url(image.value(QStringLiteral("url")).toString());
        if (!url.isValid() || url.isEmpty()) {
            continue;
        }
        const QJsonValue width = image.value(QStringLiteral("width"));
        const int distance = std::abs((width.isDouble() ? width.toInt() : preferredWidth) - preferredWidth);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = url;
        }
    }
    return best;
}

Profile profile(const QJsonObject &user)
{
    const QString name = string(user, "display_name");
    return {
        string(user, "id"),
        name.isEmpty() ? QStringLiteral("Spotify User") : name,
        preferredArtworkUrl(user.value(QStringLiteral("images")).toArray(), 192),
    };
}

Playlist playlist(const QJsonObject &object)
{
    Playlist result;
    result.id = string(object, "id");
    result.name = string(object, "name");
    result.description = plainText(string(object, "description"));
    const QJsonObject items = object.value(QStringLiteral("items")).toObject();
    const QJsonObject tracks = object.value(QStringLiteral("tracks")).toObject();
    result.songCount = items.contains(QStringLiteral("total"))
        ? items.value(QStringLiteral("total")).toInt()
        : tracks.value(QStringLiteral("total")).toInt();
    result.artworkUrl = preferredArtworkUrl(object.value(QStringLiteral("images")).toArray(), 640);
    const QJsonObject owner = object.value(QStringLiteral("owner")).toObject();
    result.ownerId = string(owner, "id");
    result.ownerName = string(owner, "display_name");
    result.isPublic = object.value(QStringLiteral("public")).toBool();
    result.isCollaborative = object.value(QStringLiteral("collaborative")).toBool();
    result.uri = string(object, "uri");
    if (result.uri.isEmpty()) {
        result.uri = QStringLiteral("spotify:playlist:") + result.id;
    }
    return result;
}

std::optional<Album> album(const QJsonObject &object)
{
    const QString id = string(object, "id");
    const QString uri = string(object, "uri");
    if (id.isEmpty() || uri.isEmpty()) {
        return std::nullopt;
    }
    Album result;
    result.id = id;
    result.name = string(object, "name");
    result.artists = joinedNames(object.value(QStringLiteral("artists")).toArray());
    result.artworkUrl = preferredArtworkUrl(object.value(QStringLiteral("images")).toArray(), 640);
    result.releaseDate = string(object, "release_date");
    result.songCount = object.value(QStringLiteral("total_tracks")).toInt();
    result.uri = uri;
    return result;
}

std::optional<Artist> artist(const QJsonObject &object)
{
    const QString id = string(object, "id");
    const QString uri = string(object, "uri");
    if (id.isEmpty() || uri.isEmpty()) {
        return std::nullopt;
    }
    return Artist {
        id,
        string(object, "name"),
        preferredArtworkUrl(object.value(QStringLiteral("images")).toArray(), 640),
        uri,
    };
}

std::optional<Song> track(const QJsonObject &object, const Album &fallbackAlbum)
{
    const QString type = string(object, "type");
    // a market lets spotify send a relinked copy in its place. playlists and
    // the library hold the original, and librespot finds the copy itself
    const QString linkedUri = string(object.value(QStringLiteral("linked_from")).toObject(), "uri");
    const QString uri = linkedUri.isEmpty() ? string(object, "uri") : linkedUri;
    if ((!type.isEmpty() && type != QLatin1StringView("track"))
        || !uri.startsWith(QLatin1StringView("spotify:track:"))) {
        return std::nullopt;
    }

    const QJsonObject albumObject = object.value(QStringLiteral("album")).toObject();
    const std::optional<Album> trackAlbum = album(albumObject);
    const Album resolvedAlbum = trackAlbum.value_or(fallbackAlbum);
    const QJsonArray artistArray = object.value(QStringLiteral("artists")).toArray();

    Song song;
    song.name = string(object, "name");
    song.artists = joinedNames(artistArray);
    song.albumName = resolvedAlbum.name;
    song.artworkUrl = preferredArtworkUrl(albumObject.value(QStringLiteral("images")).toArray(), 640);
    if (song.artworkUrl.isEmpty()) {
        song.artworkUrl = resolvedAlbum.artworkUrl;
    }
    song.uri = uri;
    song.durationMs = object.value(QStringLiteral("duration_ms")).toInt();
    song.isExplicit = object.value(QStringLiteral("explicit")).toBool();
    song.albumId = resolvedAlbum.id;
    song.albumUri = resolvedAlbum.uri;
    // only sent when the request names a market
    song.isPlayable = object.value(QStringLiteral("is_playable")).toBool(true);
    for (const QJsonValue &value : artistArray) {
        if (const auto parsed = artist(value.toObject())) {
            song.artistItems.append(*parsed);
        }
    }
    return song;
}

std::optional<Song> wrappedTrack(const QJsonObject &entry)
{
    const QJsonValue item = entry.value(QStringLiteral("item"));
    if (item.isObject()) {
        return track(item.toObject());
    }
    const QJsonValue legacy = entry.value(QStringLiteral("track"));
    return legacy.isObject() ? track(legacy.toObject()) : std::nullopt;
}

std::optional<ConnectDevice> connectDevice(const QJsonObject &object)
{
    const QString id = object.value(QStringLiteral("id")).toString();
    const QString name = object.value(QStringLiteral("name")).toString();
    if (id.isEmpty() || name.isEmpty()) {
        return std::nullopt;
    }
    ConnectDevice device;
    device.id = id;
    device.name = name;
    device.type = object.value(QStringLiteral("type")).toString();
    device.isActive = object.value(QStringLiteral("is_active")).toBool();
    device.isPrivateSession = object.value(QStringLiteral("is_private_session")).toBool();
    device.isRestricted = object.value(QStringLiteral("is_restricted")).toBool();
    device.supportsVolume = object.value(QStringLiteral("supports_volume")).toBool();
    device.volumePercent = std::clamp(object.value(QStringLiteral("volume_percent")).toInt(), 0, 100);
    return device;
}

RemotePlaybackState remotePlaybackState(const QJsonObject &object)
{
    RemotePlaybackState state;
    if (object.isEmpty()) {
        return state;
    }
    state.hasState = true;
    const QJsonValue deviceValue = object.value(QStringLiteral("device"));
    if (deviceValue.isObject()) {
        state.device = connectDevice(deviceValue.toObject());
    }
    const QJsonValue itemValue = object.value(QStringLiteral("item"));
    if (itemValue.isObject()) {
        state.item = track(itemValue.toObject());
    }
    state.isPlaying = object.value(QStringLiteral("is_playing")).toBool();
    state.progressMs = std::max(0, object.value(QStringLiteral("progress_ms")).toInt());
    state.shuffleState = object.value(QStringLiteral("shuffle_state")).toBool();
    state.repeatState = object.value(QStringLiteral("repeat_state")).toString(QStringLiteral("off"));
    return state;
}

} // namespace swiftify::json
