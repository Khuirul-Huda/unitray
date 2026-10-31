Name:           unitray
Version:        0.0.1
Release:        1%{?dist}
Summary:        Lightweight systemd service manager in your Linux system tray
License:        MIT
URL:            https://github.com/Khuirul-Huda/unitray
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  rust
BuildRequires:  cargo
BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  qt6-qtbase-devel
Requires:       systemd
Requires:       qt6-qtbase
Requires:       qt6-qtbase-gui

%description
Unitray is a fast, lightweight Linux system tray controller for managing
development services (MariaDB, PostgreSQL, Docker, etc.) on KDE Plasma,
GNOME, Waybar, and other FreeDesktop environments.

The tray daemon is written in Rust for minimal resource usage, and the
native configuration UI is written in C++ Qt 6 to seamlessly adapt to
your desktop environment style (KDE Breeze, GNOME, etc.).

%prep
%autosetup

%build
cargo build --release
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j%{?_smp_mflags}

%install
install -D -m 0755 target/release/%{name} %{buildroot}%{_bindir}/%{name}
install -D -m 0755 build/%{name}-settings %{buildroot}%{_bindir}/%{name}-settings
install -D -m 0644 data/%{name}.desktop %{buildroot}%{_datadir}/applications/%{name}.desktop
install -D -m 0644 data/%{name}-settings.desktop %{buildroot}%{_datadir}/applications/%{name}-settings.desktop
install -D -m 0644 data/%{name}.desktop %{buildroot}%{_sysconfdir}/xdg/autostart/%{name}.desktop
install -D -m 0644 data/%{name}.service %{buildroot}%{_userunitdir}/%{name}.service
install -D -m 0644 data/49-%{name}.rules %{buildroot}%{_datadir}/polkit-1/rules.d/49-%{name}.rules
install -D -m 0644 config/%{name}.toml %{buildroot}%{_sysconfdir}/%{name}/config.toml
install -D -m 0644 data/%{name}.png %{buildroot}%{_datadir}/pixmaps/%{name}.png
for s in 16 24 32 48 64 128 256 512; do
    install -D -m 0644 data/icons/hicolor/${s}x${s}/apps/%{name}.png %{buildroot}%{_datadir}/icons/hicolor/${s}x${s}/apps/%{name}.png
done

%files
%license LICENSE
%doc README.md
%{_bindir}/%{name}
%{_bindir}/%{name}-settings
%{_datadir}/applications/%{name}.desktop
%{_datadir}/applications/%{name}-settings.desktop
%{_sysconfdir}/xdg/autostart/%{name}.desktop
%{_datadir}/pixmaps/%{name}.png
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_userunitdir}/%{name}.service
%{_datadir}/polkit-1/rules.d/49-%{name}.rules
%config(noreplace) %{_sysconfdir}/%{name}/config.toml

%changelog
* Sat Oct 31 2026 Khuirul Huda <hello@khuirulhuda.my.id> - 0.0.1-1
- Initial alpha release for Fedora Linux
