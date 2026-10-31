Name:           unitray
Version:        0.1.0
Release:        1%{?dist}
Summary:        Lightweight systemd service manager in your Linux system tray
License:        MIT
URL:            https://github.com/Khuirul-Huda/unitray
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  rust
BuildRequires:  cargo
Requires:       systemd

%description
Unitray is a fast, lightweight Linux system tray controller for managing
development services (MariaDB, PostgreSQL, Docker, etc.) on KDE Plasma,
GNOME, Waybar, and other FreeDesktop environments.

%prep
%autosetup

%build
cargo build --release

%install
install -D -m 0755 target/release/%{name} %{buildroot}%{_bindir}/%{name}
install -D -m 0644 data/%{name}.desktop %{buildroot}%{_datadir}/applications/%{name}.desktop
install -D -m 0644 data/%{name}.service %{buildroot}%{_userunitdir}/%{name}.service
install -D -m 0644 data/49-%{name}.rules %{buildroot}%{_datadir}/polkit-1/rules.d/49-%{name}.rules
install -D -m 0644 config/%{name}.toml %{buildroot}%{_sysconfdir}/%{name}/config.toml

%files
%license LICENSE
%doc README.md
%{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_userunitdir}/%{name}.service
%{_datadir}/polkit-1/rules.d/49-%{name}.rules
%config(noreplace) %{_sysconfdir}/%{name}/config.toml

%changelog
* Sat Oct 31 2026 Khuirul Huda <hello@khuirulhuda.my.id> - 0.1.0-1
- Initial release for Fedora Linux
