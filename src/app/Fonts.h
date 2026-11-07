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

#include <QString>

namespace swiftify::fonts {

/// makes "Material Symbols Rounded" available. end4 installs it system-wide, so
/// that copy wins; otherwise the one installed beside swiftify (or left in the
/// build tree) is loaded.
void loadIconFont();
/// "Material Symbols Rounded"
QString iconFamily();

/// makes google sans flex available, from the system or beside swiftify:
/// the interface's face, and its titles drawn round through its variable
/// axes (TitleText). before interfaceFamily() is first asked.
void loadInterfaceFont();

/// makes Gveret Levin (the lyrics' face for hebrew) and Fredoka (hebrew
/// everywhere else) available, from the system or beside swiftify
void loadHebrewFont();

/// the first installed face of end4's google sans flex, google sans, rubik,
/// inter and noto sans, or the platform default.
QString interfaceFamily();
/// the platform's own fixed-width face: a generic name like "monospace"
/// means nothing to macos, which then searches every family for it
QString fixedFamily();

} // namespace swiftify::fonts
