use ksni::menu::{MenuItem, StandardItem};
use ksni::Tray;
use crate::config::AppConfig;
use crate::service;
use std::time::SystemTime;

#[derive(Debug, Clone)]
pub struct MonitoredService {
    pub name: String,
    pub unit: String,
    pub is_active: bool,
}

#[derive(Debug, Clone)]
pub struct UnitrayItem {
    pub icon_name: String,
    pub services: Vec<MonitoredService>,
    pub last_config_mtime: Option<SystemTime>,
}

impl UnitrayItem {
    pub fn from_config(config: &AppConfig) -> Self {
        let mtime = crate::config::resolve_config_path()
            .and_then(|p| std::fs::metadata(p).ok())
            .and_then(|m| m.modified().ok());

        let mut tray_item = Self {
            icon_name: config.tray_icon.clone(),
            services: config
                .services
                .iter()
                .map(|entry| MonitoredService {
                    name: entry.name.clone(),
                    unit: entry.unit.clone(),
                    is_active: false,
                })
                .collect(),
            last_config_mtime: mtime,
        };
        tray_item.refresh_states();
        tray_item
    }

    pub fn reload_config(&mut self) {
        let config = crate::config::load_configuration();
        self.icon_name = config.tray_icon;
        self.services = config
            .services
            .iter()
            .map(|entry| MonitoredService {
                name: entry.name.clone(),
                unit: entry.unit.clone(),
                is_active: false,
            })
            .collect();
        self.refresh_states();
    }

    pub fn refresh_states(&mut self) -> bool {
        let mut has_changes = false;
        for item in &mut self.services {
            let active = service::is_service_active(&item.unit);
            if item.is_active != active {
                item.is_active = active;
                has_changes = true;
            }
        }
        has_changes
    }
}

impl Tray for UnitrayItem {
    const MENU_ON_ACTIVATE: bool = true;

    fn id(&self) -> String {
        "unitray".into()
    }

    fn title(&self) -> String {
        let active_count = self.services.iter().filter(|s| s.is_active).count();
        format!("Unitray ({} active)", active_count)
    }

    fn icon_theme_path(&self) -> String {
        if let Some(data_dir) = dirs::data_dir() {
            let user_icons = data_dir.join("icons");
            if user_icons.exists() {
                return user_icons.to_string_lossy().to_string();
            }
        }
        String::new()
    }

    fn icon_name(&self) -> String {
        self.icon_name.clone()
    }

    fn icon_pixmap(&self) -> Vec<ksni::Icon> {
        if self.icon_name == "unitray" {
            load_embedded_unitray_pixmaps()
        } else {
            Vec::new()
        }
    }

    fn menu_about_to_show(&mut self) {
        let current_mtime = crate::config::resolve_config_path()
            .and_then(|p| std::fs::metadata(p).ok())
            .and_then(|m| m.modified().ok());

        if current_mtime != self.last_config_mtime {
            self.reload_config();
            self.last_config_mtime = current_mtime;
        } else {
            self.refresh_states();
        }
    }

    fn menu(&self) -> Vec<MenuItem<Self>> {
        let mut items: Vec<MenuItem<Self>> = Vec::new();

        for service in &self.services {
            let (status_icon, status_text) = if service.is_active {
                ("🟢", "Running")
            } else {
                ("🔴", "Stopped")
            };
            let label = format!("{}  [{} {}]", service.name, status_icon, status_text);
            let unit_name = service.unit.clone();
            let is_running = service.is_active;

            items.push(
                StandardItem {
                    label,
                    enabled: true,
                    activate: Box::new(move |this: &mut Self| {
                        if is_running {
                            let _ = service::stop_service(&unit_name);
                        } else {
                            let _ = service::start_service(&unit_name);
                        }
                        this.refresh_states();
                    }),
                    ..Default::default()
                }
                .into(),
            );
        }

        items.push(MenuItem::Separator);

        items.push(
            StandardItem {
                label: "Restart All Services".into(),
                icon_name: "view-refresh".into(),
                activate: Box::new(|this: &mut Self| {
                    for s in &this.services {
                        let _ = service::restart_service(&s.unit);
                    }
                    this.refresh_states();
                }),
                ..Default::default()
            }
            .into(),
        );

        items.push(
            StandardItem {
                label: "Stop All Services".into(),
                icon_name: "process-stop".into(),
                activate: Box::new(|this: &mut Self| {
                    for s in &this.services {
                        let _ = service::stop_service(&s.unit);
                    }
                    this.refresh_states();
                }),
                ..Default::default()
            }
            .into(),
        );

        items.push(MenuItem::Separator);

        items.push(
            StandardItem {
                label: "Settings...".into(),
                icon_name: "configure".into(),
                activate: Box::new(|_| {
                    open_settings_ui();
                }),
                ..Default::default()
            }
            .into(),
        );

        items.push(
            StandardItem {
                label: "About Unitray...".into(),
                icon_name: "help-about".into(),
                activate: Box::new(|_| {
                    open_about_ui();
                }),
                ..Default::default()
            }
            .into(),
        );

        items.push(
            StandardItem {
                label: "Quit Unitray".into(),
                icon_name: "application-exit".into(),
                activate: Box::new(|_| std::process::exit(0)),
                ..Default::default()
            }
            .into(),
        );

        items
    }
}

fn resolve_settings_binary() -> std::path::PathBuf {
    if let Ok(current_exe) = std::env::current_exe() {
        if let Some(parent) = current_exe.parent() {
            let candidate = parent.join("unitray-settings");
            if candidate.exists() {
                return candidate;
            }
            let candidate_build = parent.join("../build/unitray-settings");
            if candidate_build.exists() {
                return candidate_build;
            }
        }
    }

    let standard_paths = [
        dirs::home_dir().map(|h| h.join(".local/bin/unitray-settings")),
        Some(std::path::PathBuf::from("/usr/local/bin/unitray-settings")),
        Some(std::path::PathBuf::from("/usr/bin/unitray-settings")),
    ];

    for path_opt in standard_paths.into_iter().flatten() {
        if path_opt.exists() {
            return path_opt;
        }
    }

    std::path::PathBuf::from("unitray-settings")
}

fn launch_gui_process(args: &[&str]) {
    let args_owned: Vec<String> = args.iter().map(|s| s.to_string()).collect();
    std::thread::spawn(move || {
        let binary = resolve_settings_binary();
        if let Ok(mut child) = std::process::Command::new(&binary).args(&args_owned).spawn() {
            let _ = child.wait();
            return;
        }

        if args_owned.is_empty() {
            if let Some(config_path) = crate::config::resolve_config_path() {
                let _ = std::process::Command::new("xdg-open")
                    .arg(config_path)
                    .spawn();
            }
        }
    });
}

fn open_settings_ui() {
    launch_gui_process(&[]);
}

fn open_about_ui() {
    launch_gui_process(&["--about"]);
}

fn load_embedded_unitray_pixmaps() -> Vec<ksni::Icon> {
    const ICON_PNG_22: &[u8] = include_bytes!("../data/icons/hicolor/22x22/apps/unitray.png");
    const ICON_PNG_32: &[u8] = include_bytes!("../data/icons/hicolor/32x32/apps/unitray.png");

    let mut result = Vec::new();
    for bytes in [ICON_PNG_22, ICON_PNG_32] {
        if let Ok(dyn_img) = image::load_from_memory(bytes) {
            let rgba = dyn_img.to_rgba8();
            let width = rgba.width() as i32;
            let height = rgba.height() as i32;
            let mut argb_data = Vec::with_capacity((width * height * 4) as usize);
            for pixel in rgba.pixels() {
                // ksni::Icon specifies ARGB32 network byte order (Big Endian: [A, R, G, B])
                argb_data.push(pixel[3]); // Alpha
                argb_data.push(pixel[0]); // Red
                argb_data.push(pixel[1]); // Green
                argb_data.push(pixel[2]); // Blue
            }
            result.push(ksni::Icon {
                width,
                height,
                data: argb_data,
            });
        }
    }
    result
}
