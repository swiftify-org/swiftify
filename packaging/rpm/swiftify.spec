# the build downloads corrosion, the fonts and the rust crates, so it needs
# the network
Name:           swiftify
Version:        0.25.0
Release:        1%{?dist}
Summary:        A Spotify player for Wayland, in Material 3 after end4
License:        GPL-3.0-or-later AND Apache-2.0 AND BSD-3-Clause AND MIT AND MPL-2.0 AND OFL-1.1 AND Unicode-3.0 AND Zlib
URL:            https://github.com/swiftify-org/swiftify
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

# off like in the arch package
%global _lto_cflags %{nil}

BuildRequires:  cmake >= 3.24
BuildRequires:  gcc-c++
BuildRequires:  git-core
BuildRequires:  cargo
BuildRequires:  rust >= 1.85
BuildRequires:  desktop-file-utils
BuildRequires:  cmake(Qt6Core) >= 6.10
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6Concurrent)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6QuickControls2)
BuildRequires:  cmake(Qt6ShaderTools)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(Qt6WebSockets)
BuildRequires:  cmake(Qt6Keychain)
BuildRequires:  cmake(QCoro6Core)
BuildRequires:  cmake(QCoro6Network)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(alsa)
# libressl provides pkgconfig(openssl) on opensuse too, and openssl-sys
# rejects libressl newer than 4.3
%if 0%{?suse_version}
BuildRequires:  libopenssl-devel
%else
BuildRequires:  pkgconfig(openssl)
%endif

Requires:       hicolor-icon-theme
Recommends:     xdg-desktop-portal

%if 0%{?suse_version}
# opensuse leaves the certificates for the downloads out of the build root
BuildRequires:  ca-certificates-mozilla
# fedora pulls the qml imports and the wayland plugin in through the qt
# libraries, opensuse packages them apart
Requires:       qt6-declarative-imports
Requires:       qt6-wayland
%endif

%description
A Spotify player for Wayland, in Material 3 Expressive after end4. It plays
through librespot and hooks into MPRIS for media keys and shell widgets.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

%files
%dir %{_datadir}/licenses/%{name}
%license %{_datadir}/licenses/%{name}/*
%{_bindir}/%{name}
%{_datadir}/%{name}/
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
