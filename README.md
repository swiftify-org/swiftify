<div align="center">

<img src="packaging/icons/swiftify-256.png" width="128" alt="swiftify icon">

# Swiftify

a spotify player for desktop, built with Qt Quick on top of a Rust core that wraps [librespot](https://github.com/librespot-org/librespot)

[![release](https://img.shields.io/github/v/release/swiftify-org/swiftify?style=for-the-badge&color=6750a4&labelColor=1d1b20)](https://github.com/swiftify-org/swiftify/releases/latest)
[![downloads](https://img.shields.io/github/downloads/swiftify-org/swiftify/total?style=for-the-badge&color=6750a4&labelColor=1d1b20)](https://github.com/swiftify-org/swiftify/releases)
[![stars](https://img.shields.io/github/stars/swiftify-org/swiftify?style=for-the-badge&color=6750a4&labelColor=1d1b20)](https://github.com/swiftify-org/swiftify/stargazers)
[![license](https://img.shields.io/github/license/swiftify-org/swiftify?style=for-the-badge&color=6750a4&labelColor=1d1b20)](LICENSE)

[![aur](https://img.shields.io/aur/version/swiftify-bin?style=for-the-badge&logo=archlinux&logoColor=white&label=aur&color=1793d1&labelColor=1d1b20)](https://aur.archlinux.org/packages/swiftify-bin)
[![copr](https://img.shields.io/badge/copr-shredos%2Fswiftify-51a2da?style=for-the-badge&logo=fedora&logoColor=white&labelColor=1d1b20)](https://copr.fedorainfracloud.org/coprs/shredos/swiftify/)
[![qt](https://img.shields.io/badge/Qt_Quick-6.10-41cd52?style=for-the-badge&logo=qt&logoColor=white&labelColor=1d1b20)](https://www.qt.io)
[![rust](https://img.shields.io/badge/core-rust-dea584?style=for-the-badge&logo=rust&logoColor=white&labelColor=1d1b20)](spotify-core)

**[download](https://shredos.dev/#download)** · **[install](#install)** · **[before you start](#before-you-start)** · **[guides](#guides)** · **[build from source](#building-from-source)**

<img src="assets/home.png" alt="swiftify home view">

</div>

## about

it follows Material 3 Expressive (heavy inspo from end4) and has an immersive player,
synced lyrics, visualizers, an equalizer, and local files with local playlists
where u can edit the tracks metadata.

this apps purpose is to replace spotifys heavy webview client, but its also mostly eyecandy lol.
it runs significantly lighter than the spotify client.

**EVEN WITH THE EYECANDY TURNED ON....** it is lighter than spotify + spotifys renderers both on cpu AND ram!!!!!

id recommend turning the visualizers and background off if the app runs in the background tho lol

***anyways runs on linux (made for wayland and hyprland), macos and windows.***

<table>
  <tr>
    <td><img src="assets/eq.png" alt="equalizer, tempo and pitch settings" width="400"></td>
    <td><img src="assets/immersive.png" alt="immersive player with the visualizer and lyrics" width="400"></td>
  </tr>
  <tr>
    <td align="center">equalizer, tempo and pitch</td>
    <td align="center">immersive player with the visualizer and lyrics</td>
  </tr>
</table>

## install

> [!IMPORTANT]
> swiftify needs **spotify premium** and **your own spotify client id**, see [before you start](#before-you-start)

> [!NOTE]
> on linux your login gets saved in the secret service through QtKeychain, so u need something
> providing one, like gnome-keyring, kwallet or keepassxc

<details>
<summary><b>arch linux</b></summary>
<br>

all three are on the aur, install one with paru or yay

| package | |
|---|---|
| [`swiftify-bin`](https://aur.archlinux.org/packages/swiftify-bin) | the prebuilt release binary (recommended) |
| [`swiftify`](https://aur.archlinux.org/packages/swiftify) | builds the latest release from source |
| [`swiftify-git`](https://aur.archlinux.org/packages/swiftify-git) | builds the latest commit on main |

```sh
paru -S swiftify-bin
```

</details>

<details>
<summary><b>fedora and opensuse tumbleweed</b></summary>
<br>

the [copr](https://copr.fedorainfracloud.org/coprs/shredos/swiftify/) gets a build of every release for x86_64 and aarch64

on fedora

```sh
sudo dnf copr enable shredos/swiftify
sudo dnf install swiftify
```

on opensuse tumbleweed

```sh
sudo zypper addrepo https://copr.fedorainfracloud.org/coprs/shredos/swiftify/repo/opensuse-tumbleweed/shredos-swiftify-opensuse-tumbleweed.repo
sudo zypper refresh
sudo zypper install swiftify
```

updates come in with the rest of your system updates

</details>

<details>
    <summary><b>gentoo linux</b></summary>
<br>

via the [ahhh_overlay](https://github.com/AhhhSaturn/ahhh_overlay/tree/main/media-sound/swiftify)

```sh
eselect repository enable ahhh_overlay
emaint sync --repo ahhh_overlay
emerge media-sound/swiftify
```

</details>

<details>
<summary><b>other linux</b></summary>
<br>

[![download for linux](https://img.shields.io/badge/download-linux_x86__64-6750a4?style=for-the-badge&logo=linux&logoColor=white&labelColor=1d1b20)](https://github.com/swiftify-org/swiftify/releases/latest)

grab the `linux-x86_64.tar.gz` from the latest release. it brings its own Qt, but needs alsa and libsecret,
which pretty much every desktop already has. unpack it anywhere and run `bin/swiftify` inside it

```sh
tar -xzf swiftify-*-linux-x86_64.tar.gz
./swiftify-*-linux-x86_64/bin/swiftify
```

to update, quit swiftify, unpack the new release and delete the old folder

</details>

<details>
<summary><b>macos</b></summary>
<br>

[![download for macos](https://img.shields.io/badge/download-macos_apple_silicon-6750a4?style=for-the-badge&logo=apple&logoColor=white&labelColor=1d1b20)](https://github.com/swiftify-org/swiftify/releases/latest)

grab the `macos-arm64.dmg` from the latest release. it needs an apple silicon mac on macos 14.4 or newer

1. open the dmg and drag swiftify into applications
2. its signed ad hoc, so macos blocks the first launch. open system settings › privacy & security
   and click open anyway, or run
   ```sh
   xattr -dr com.apple.quarantine /Applications/Swiftify.app
   ```
3. after that it opens like any other app

to update, quit swiftify and drag the new version into applications over the old one

</details>

<details>
<summary><b>windows</b></summary>
<br>

[![download for windows](https://img.shields.io/badge/download-windows_x64-6750a4?style=for-the-badge&labelColor=1d1b20)](https://github.com/swiftify-org/swiftify/releases/latest)

grab the `windows-x86_64.zip` from the latest release. it brings everything it needs, so there is no installer

1. extract the zip and move the folder inside it somewhere it can stay, like `%LOCALAPPDATA%\Programs\Swiftify`
2. run `bin\swiftify.exe` in that folder
3. the exe isnt signed, so if smartscreen says windows protected your pc, click more info and then run anyway
4. to get it in the start menu, right click `swiftify.exe` and pin it to start

to update, quit the running copy through its tray menu, then copy the new folder's contents over the old one

</details>

u can also get every build from [shredos.dev](https://shredos.dev/#download)

## before you start

- **spotify premium.** playback goes through librespot, and that only works with premium.
- **a spotify client id.** your library, playlists and search come from the spotify
  web api through your own developer app. the first launch walks u through it.
  make an app in the [spotify developer dashboard](https://developer.spotify.com/dashboard),
  add `http://127.0.0.1:8898/login` as a redirect uri, and paste the apps client id
  into swiftify. you dont need the client secret.

## guides

### spotify soloist

> [!TIP]
> this is the solution of 80% of problems, and often also for the error message "Spotify will not play this track"

spotify refuses librespot the keys to decrypt songs mostly on newer accounts (librespot's
logs say `error audio key 0 1`), so every song fails. swiftify can play
spotify's songs through [spotify soloist](https://developer.spotify.com/documentation/soloist) instead, spotify's own connect client, which isnt affected.

<details>
<summary><b>set up soloist</b></summary>
<br>

1. [download the soloist binary from spotify](https://developer.spotify.com/documentation/soloist/tutorials/getting-started#download-spotify-soloist)
2. extract the downloaded .zip file and
   1. WINDOWS: move `soloist.exe` to the exact same folder your `swiftify.exe` is in
   2. MACOS or LINUX: move the soloist binary to `~/.local/bin/`, `/opt/homebrew/bin/` or anywhere on your `PATH`
3. go to the [spotify developer soloist dashboard](https://developer.spotify.com/dashboard/soloist) and generate and copy your API key from there.
4. create a text file in `%LOCALAPPDATA%\soloist` on windows or `~/.config/soloist/` called `api-key`. you can also put your api key inside of `SOLOIST_API_KEY` (this path and filename must match exactly this. please really check this twice as windows loves to append ".txt" to text files and doesnt show it)
5. open that file with a text editor and paste your previously copied api key into this file.
6. save the file.
7. open spotify on your phone or on your pc and go to the devices popup
8. open swiftify and try to play something. you should get an error saying you need to connect soloist.
9. a "Swiftify" device should show up in the devices in the native spotify app. Click that. if it doesnt show up fully restart swiftify and try again.
10. once it connects try to play anything. Congratulations you just set up Spotify soloist

you can disable soloist manually with `SWIFTIFY_PLAYBACK=librespot`

if spotify doesnt list "Swiftify" for step 9, please retry or a firewall (ufw) is probably
dropping the pairing connection. pair from the spotify desktop app on the same computer.

</details>

### discord rich presence

enable "Discord Rich Presence" inside the swiftify account settings to display your current track, artist, public album cover
and playback status on your Discord profile. the Discord app needs to be running for this to work. I recommend disabling
"Display spotify as your status" if you have connected your Spotify account so it doesnt show twice.

this integration works with the native app, Canary builds, Beta builds, Vesktop and app stores like Flatpak or Snap

<details>
<summary><b>set up discord rpc</b></summary>
<br>

1. create an application on [Discord's Developer Portal](https://discord.com/developers/applications)
2. copy its public AppID (this is just an identifier for your app. this is not confidential dont worry about that)
3. paste your AppID into the Application ID field in the swiftify account settings.

</details>

## command line

only one swiftify runs at a time. launching it again just passes the command to the one
that's already running, so these work great as keybinds

| flag           |                           |
|----------------|---------------------------|
| `--immersive`  | open the immersive player |
| `--play-pause` | play or pause             |
| `--next`       | next song                 |
| `--previous`   | previous song             |
| `--stats`      | open the stats page       |

on hyprland for example

```ini
bind = SUPER SHIFT, M, exec, swiftify --immersive
bind = , XF86AudioPlay, exec, swiftify --play-pause
```

## building from source

- Qt 6.10 or newer with Qt Base, Qt Declarative, Qt Shader Tools and Qt WebSockets
- QtKeychain and QCoro 0.10 or newer, both built for Qt 6
- CMake 3.24 or newer, Ninja, and a C++20 compiler
- Rust (stable, from [rustup](https://rustup.rs))

the first cmake run needs internet, it pulls [Corrosion](https://github.com/corrosion-rs/corrosion)
(which builds the rust core as part of the cmake build) and two fonts, Material Symbols
Rounded and Google Sans Flex, each checked against a pinned hash.

the builds below skip the tests with `-DBUILD_TESTING=OFF`. for a manual CMake
build, drop that and run `ctest --test-dir build` if u want them.

<details>
<summary><b>macos</b></summary>
<br>

u need [homebrew](https://brew.sh) and apple's command line tools (`xcode-select --install`, or full xcode). then run

```sh
brew install qtbase qtdeclarative qtshadertools qtwebsockets qtkeychain qcoro6 cmake ninja
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"

git clone https://github.com/swiftify-org/swiftify.git
cd swiftify
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build
ditto build/swiftify.app /Applications/Swiftify.app
```

it's signed ad hoc, so no developer account needed.
with full xcode installed the icon gets compiled with `actool` and picks up macos 26's glass,
dark and tinted looks. with just the command line tools it's built from the PNGs.

</details>

<details>
<summary><b>linux</b></summary>
<br>

build an arch package from this checkout

```sh
git clone https://github.com/swiftify-org/swiftify.git
cd swiftify/packaging/arch
makepkg -si
```

or use cmake

```sh
# replace with your package manager if not on arch
sudo pacman -S --needed git qt6-base qt6-declarative qt6-shadertools qt6-wayland qt6-websockets qtkeychain-qt6 qcoro libpulse alsa-lib openssl cmake ninja rust pkgconf

git clone https://github.com/swiftify-org/swiftify.git
cd swiftify
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

on debian/ubuntu (or derivatives with Qt 6.8 or 6.9), use the build script that handles
compatibility patches automatically

```sh
sudo apt-get install git qt6-base-dev qt6-declarative-dev qt6-shader-baker \
    qt6-shadertools-dev qt6-wayland qt6-websockets-dev qtkeychain-qt6-dev qcoro-qt6-dev \
    libpulse-dev libasound2-dev libssl-dev cmake ninja-build pkg-config
# install rust from https://rustup.rs if you dont have it

git clone https://github.com/swiftify-org/swiftify.git
cd swiftify
./tools/build-debian.sh
```

or with `--install-deps` to install the dependencies automatically

```sh
git clone https://github.com/swiftify-org/swiftify.git
cd swiftify
./tools/build-debian.sh --install-deps
```

anywhere else, install the same reqs (Qt 6.10+ with D-Bus, OpenGL and Wayland, QtKeychain
and QCoro for Qt 6, libpulse, plus the ALSA, OpenSSL and pkg-config dev packages), then
run the cmake commands above. if you have Qt 6.8 or 6.9, use `tools/build-debian.sh` which
applies the necessary compatibility patches.

`cmake --install` drops `swiftify` in `~/.local/bin` (make sure that's on your `PATH`)
and the desktop entry, icons and fonts in `~/.local/share`.

on linux it also hooks into MPRIS (media keys, `playerctl`, shell media widgets),
logind (pauses before the machine sleeps), pipewire/pulseaudio device changes, and
the tray. `-DSWIFTIFY_DESKTOP_INTEGRATION=OFF` turns those off.

</details>

<details>
<summary><b>windows</b></summary>
<br>

**because most Windows users cant even use their own OS, i made a script that handles building so it's as simple as possible :)**

install these

- **Qt 6.10** with *Qt Shader Tools* and *Qt WebSockets*, **CMake** and **Ninja** from the **Qt online Installer**
- **Rust** from [rustup](https://rustup.rs)
- **git** from [git-scm.com](https://git-scm.com/install/)

choose either **Qt for MSVC 2022 64-bit** with Visual Studio 2022 17.1+ (or Build Tools) and its *Desktop development with C++*
workload, or **Qt for MinGW 64-bit** with its matching MinGW toolchain.

then run this in Windows PowerShell

```powershell
git clone https://github.com/swiftify-org/swiftify.git
Set-Location swiftify
.\tools\build-windows.ps1
```

for a different Qt kit, toolchain location or install directory, pass overrides

```powershell
.\tools\build-windows.ps1 -QtPath 'C:\Qt\6.11.2\msvc2022_64'
.\tools\build-windows.ps1 -QtPath 'C:\Qt\6.10.3\mingw_64' -MingwPath 'C:\Qt\Tools\mingw1310_64\bin' -InstallPrefix "$env:LOCALAPPDATA\Programs\Swiftify"
```

adjust the example paths to your installed versions, MinGW must match your Qt
kit, not just the example folder name.

**the toolchain decides one feature.** the Windows media controls (the media overlay,
lock screen and media keys showing what's playing) need `SystemMediaTransportControls`
in the compiler's Windows headers.

| toolchain                                               | media controls            |
|---------------------------------------------------------|---------------------------|
| MSVC 2022                                               | yes                       |
| Qt's MinGW 13.1 (`mingw1310_64`, mingw-w64 runtime v11) | no, everything else works |
| a newer mingw-w64 (tested with GCC 16.1, runtime v14)   | yes                       |

when they're left out, cmake warns `building without the Windows media controls`.
for every feature, build with MSVC. a newer MinGW works too, but it isn't the one
your Qt kit was built with, so the script warns about the mismatch.

**quit a running installed copy through its tray menu before updating it.**

if PowerShell blocks the script, review it first, then run
`powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-windows.ps1`.
this applies only to that process, not your machine's execution policy.

start the installed app with

```powershell
& "$env:LOCALAPPDATA\Programs\Swiftify\bin\swiftify.exe"
```

font downloads are hash-checked on every configure. on Windows, a failed CMake
download is retried with `curl.exe` using the system certificate store.

</details>

## authors

Swiftify was developed by [shred0s](https://github.com/shred0s) at [github.com/swiftify-org/swiftify](https://github.com/swiftify-org/swiftify), with help from everyone below

<a href="https://github.com/swiftify-org/swiftify/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=swiftify-org/swiftify" alt="contributors">
</a>

## license

swiftify is licensed under the gnu gpl v3 or later, see [LICENSE](LICENSE)
