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

#include <QJsonArray>
#include <QJsonObject>
#include <optional>

/// spotify web api objects to swiftify's own, as SpotifyWebAPI.swift maps them.
/// an entry spotify returns malformed is skipped rather than failing the page.
namespace swiftify::json {

QUrl preferredArtworkUrl(const QJsonArray &images, int preferredWidth = 640);
Profile profile(const QJsonObject &user);
Playlist playlist(const QJsonObject &object);
std::optional<Album> album(const QJsonObject &object);
std::optional<Artist> artist(const QJsonObject &object);
/// only real tracks: episodes and local files come back as nullopt
std::optional<Song> track(const QJsonObject &object, const Album &fallbackAlbum = {});
/// the "item" of a playlist, saved-track or history entry, or its older "track"
std::optional<Song> wrappedTrack(const QJsonObject &entry);
std::optional<ConnectDevice> connectDevice(const QJsonObject &object);
RemotePlaybackState remotePlaybackState(const QJsonObject &object);

} // namespace swiftify::json
