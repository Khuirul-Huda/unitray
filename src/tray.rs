use ksni::menu::{CheckmarkItem, MenuItem, StandardItem};
use ksni::Tray;
use crate::config::AppConfig;
use crate::service;

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
}

impl UnitrayItem {
    pub fn from_config(config: &AppConfig) -> Self {
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
        };
        tray_item.refresh_states();
        tray_item
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

    fn icon_name(&self) -> String {
        self.icon_name.clone()
    }

    fn menu_about_to_show(&mut self) {
        self.refresh_states();
    }

    fn menu(&self) -> Vec<MenuItem<Self>> {
        let mut items: Vec<MenuItem<Self>> = Vec::new();

        for service in &self.services {
            let status_badge = if service.is_active { "● Running" } else { "○ Stopped" };
            let label = format!("{}  [{}]", service.name, status_badge);
            let unit_name = service.unit.clone();
            let is_running = service.is_active;

            items.push(
                CheckmarkItem {
                    label,
                    checked: is_running,
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
                label: "About Unitray...".into(),
                icon_name: "help-about".into(),
                activate: Box::new(|_| {
                    show_about_dialog();
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

fn show_about_dialog() {
    std::thread::spawn(|| {
        let title = "About Unitray";
        let content = "\
Unitray v0.1.0
Lightweight Systemd Service Controller for Linux System Tray

Author & Maintainer:
  Khuirul Huda (@khuirulhuda)
  Email: hudah2102@gmail.com
  GitHub: https://github.com/khuirulhuda/unitray

License:
  MIT License - Copyright (c) 2026 Khuirul Huda
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files, to deal in the Software
  without restriction, including without limitation the rights to use, copy,
  modify, merge, publish, distribute, sublicense, and/or sell copies.";

        // Try native KDE dialog first
        let kdialog_result = std::process::Command::new("kdialog")
            .arg("--title")
            .arg(title)
            .arg("--msgbox")
            .arg(content)
            .status();

        if let Ok(status) = kdialog_result {
            if status.success() {
                return;
            }
        }

        // Fallback to Zenity (GNOME / GTK desktop)
        let zenity_result = std::process::Command::new("zenity")
            .arg("--info")
            .arg("--title")
            .arg(title)
            .arg("--text")
            .arg(content)
            .status();

        if let Ok(status) = zenity_result {
            if status.success() {
                return;
            }
        }

        // Fallback to Desktop Notification
        let _ = std::process::Command::new("notify-send")
            .arg(title)
            .arg(content)
            .status();
    });
}
