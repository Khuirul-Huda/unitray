# Unitray 🚀

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: Rust](https://img.shields.io/badge/Language-Rust-orange.svg)](https://www.rust-lang.org/)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-blue.svg)](https://www.freedesktop.org/)

**Unitray** is an ultra-lightweight, zero-bloat system tray controller for managing local development services (`mariadb`, `postgresql`, `docker`, `cloudflared`, etc.) on Linux desktops.

Built with **Rust** using the native FreeDesktop **StatusNotifierItem (SNI)** D-Bus protocol. No heavy GUI toolkit dependencies required (No GTK, No Qt runtime needed).

---

## ✨ Features

- ⚡ **Blazing Fast & Lightweight**: Written in Rust, single binary, consumes only **~2–4 MB of RAM**.
- 🖥️ **Universal Desktop Support**: Works out of the box on **KDE Plasma 6/5**, **GNOME** (via AppIndicator), **Waybar / Hyprland / Sway**, **XFCE**, and **i3**.
- 🛠️ **Configurable via TOML**: Easily add or remove services by editing `~/.config/unitray/config.toml`.
- 🔄 **Real-Time Polling**: Status updates automatically whether services are toggled from the tray or via terminal `systemctl`.
- 🔑 **Passwordless Polkit Integration**: Toggle services instantly without annoying root password prompts.

---

## 📦 Installation

### From Source (Cargo)

```bash
git clone https://github.com/khuirulhuda/unitray.git
cd unitray
cargo build --release
sudo install -m 0755 target/release/unitray /usr/local/bin/unitray
```

### Desktop & Systemd Integration

```bash
# Install XDG desktop launcher & autostart
mkdir -p ~/.config/autostart
cp data/unitray.desktop ~/.config/autostart/

# (Optional) Install as systemd user service instead
mkdir -p ~/.config/systemd/user
cp data/unitray.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now unitray
```

---

## ⚙️ Configuration

Copy the default configuration to your user config directory:

```bash
mkdir -p ~/.config/unitray
cp config/unitray.toml ~/.config/unitray/config.toml
```

Edit `~/.config/unitray/config.toml`:

```toml
refresh_interval_secs = 3
tray_icon = "server-database"

[[services]]
name = "MariaDB"
unit = "mariadb.service"

[[services]]
name = "PostgreSQL"
unit = "postgresql.service"

[[services]]
name = "Docker"
unit = "docker.service"

[[services]]
name = "Cloudflare Tunnel"
unit = "cloudflared.service"
```

---

## 🔐 Passwordless Systemd Toggles (Polkit)

To allow toggling development services without entering your root password each time:

```bash
sudo cp data/49-unitray.rules /etc/polkit-1/rules.d/
```

---

## 📜 Packaging

- **Fedora / RHEL / openSUSE**: See [`packaging/unitray.spec`](packaging/unitray.spec)
- **Arch Linux (AUR)**: See [`packaging/PKGBUILD`](packaging/PKGBUILD)

---

## 📄 License

Licensed under the [MIT License](LICENSE). Copyright (c) 2026 Khuirul Huda.
