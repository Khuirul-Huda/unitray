use eframe::egui;
use std::collections::HashSet;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::Instant;
use unitray_core::config::{self, AppConfig, ServiceEntry};
use unitray_core::lock;
use unitray_core::service::{self, SystemdServiceInfo};

const POPULAR_TRAY_ICONS: &[(&str, &str)] = &[
    ("unitray", "Unitray 3D Clay Logo"),
    ("preferences-system-services", "System Services (Gears)"),
    ("server-database", "Database Server"),
    ("system-run", "System Run"),
    ("utilities-system-monitor", "System Monitor"),
    ("network-server", "Network Server"),
    ("applications-system", "System Applications"),
    ("systemsettings", "System Settings"),
    ("cpu", "CPU / Processor"),
    ("dialog-memory", "Memory / RAM"),
    ("internet-services", "Internet Services"),
    ("network-wired", "Network Wired"),
];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum SettingsTab {
    Monitored,
    Explorer,
    General,
}

struct UnitraySettingsApp {
    config: AppConfig,
    all_services: Vec<SystemdServiceInfo>,
    service_status_cache: std::collections::HashMap<String, bool>,
    selected_tab: SettingsTab,
    service_filter: String,
    selected_monitored_idx: Option<usize>,

    // Add custom service modal
    add_modal_open: bool,
    custom_unit_input: String,
    custom_name_input: String,

    // Icon picker modal
    icon_modal_open: bool,
    icon_search_filter: String,
    discovered_icons: Vec<String>,

    // Status notification
    status_message: Option<(String, Instant)>,
    _single_instance_guard: Option<lock::SingleInstanceGuard>,
}

impl UnitraySettingsApp {
    pub fn new(guard: lock::SingleInstanceGuard) -> Self {
        let config = config::load_configuration();
        let all_services = service::query_all_systemd_services();

        let mut status_cache = std::collections::HashMap::new();
        for s in &all_services {
            status_cache.insert(s.unit.clone(), s.is_active);
        }

        let discovered_icons = discover_system_icons();

        Self {
            config,
            all_services,
            service_status_cache: status_cache,
            selected_tab: SettingsTab::Monitored,
            service_filter: String::new(),
            selected_monitored_idx: None,
            add_modal_open: false,
            custom_unit_input: String::new(),
            custom_name_input: String::new(),
            icon_modal_open: false,
            icon_search_filter: String::new(),
            discovered_icons,
            status_message: None,
            _single_instance_guard: Some(guard),
        }
    }

    fn is_service_active(&self, unit: &str) -> bool {
        self.service_status_cache
            .get(unit)
            .copied()
            .unwrap_or_else(|| service::is_service_active(unit))
    }

    fn save_and_apply(&mut self) {
        match config::save_configuration(&self.config) {
            Ok(_) => {
                self.status_message = Some((
                    format!(
                        "Configuration saved with {} monitored services! Unitray has automatically applied changes.",
                        self.config.services.len()
                    ),
                    Instant::now(),
                ));
            }
            Err(e) => {
                self.status_message = Some((format!("Failed to save: {}", e), Instant::now()));
            }
        }
    }

    fn move_selected_service(&mut self, delta: isize) {
        if let Some(idx) = self.selected_monitored_idx {
            let new_idx = idx as isize + delta;
            if new_idx >= 0 && (new_idx as usize) < self.config.services.len() {
                let target = new_idx as usize;
                self.config.services.swap(idx, target);
                self.selected_monitored_idx = Some(target);
            }
        }
    }

    fn remove_selected_service(&mut self) {
        if let Some(idx) = self.selected_monitored_idx {
            if idx < self.config.services.len() {
                self.config.services.remove(idx);
                self.selected_monitored_idx = None;
            }
        }
    }
}

impl eframe::App for UnitraySettingsApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Top Header
        egui::TopBottomPanel::top("top_panel").show(ctx, |ui| {
            ui.add_space(8.0);
            ui.horizontal(|ui| {
                ui.heading("⚡ Unitray Settings");
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if let Some((msg, created_at)) = &self.status_message {
                        if created_at.elapsed().as_secs() < 5 {
                            ui.colored_label(egui::Color32::from_rgb(46, 204, 113), msg);
                        }
                    }
                });
            });
            ui.label("Configure monitored systemd services and tray appearance.");
            ui.add_space(6.0);

            // Tab bar styled with distinct buttons
            ui.horizontal(|ui| {
                let monitored_count = self.config.services.len();
                let explorer_count = self.all_services.len();

                ui.selectable_value(
                    &mut self.selected_tab,
                    SettingsTab::Monitored,
                    format!("📌 Monitored in Tray ({})", monitored_count),
                );
                ui.selectable_value(
                    &mut self.selected_tab,
                    SettingsTab::Explorer,
                    format!("🔍 All System Services ({})", explorer_count),
                );
                ui.selectable_value(
                    &mut self.selected_tab,
                    SettingsTab::General,
                    "⚙ General Options",
                );
            });
            ui.add_space(4.0);
        });

        // Bottom Controls Bar
        egui::TopBottomPanel::bottom("bottom_panel").show(ctx, |ui| {
            ui.add_space(6.0);
            ui.horizontal(|ui| {
                let config_path_str = config::resolve_config_path()
                    .map(|p| p.to_string_lossy().to_string())
                    .unwrap_or_else(|| "~/.config/unitray/config.toml".into());
                ui.label(format!("Config: {}", config_path_str));

                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if ui.button("💾 Save & Apply").clicked() {
                        self.save_and_apply();
                    }
                    if ui.button("Cancel").clicked() {
                        ctx.send_viewport_cmd(egui::ViewportCommand::Close);
                    }
                });
            });
            ui.add_space(6.0);
        });

        // Main Content Area
        egui::CentralPanel::default().show(ctx, |ui| {
            match self.selected_tab {
                SettingsTab::Monitored => self.render_monitored_tab(ui),
                SettingsTab::Explorer => self.render_explorer_tab(ui),
                SettingsTab::General => self.render_general_tab(ui),
            }
        });

        // Modals
        if self.add_modal_open {
            self.render_add_modal(ctx);
        }
        if self.icon_modal_open {
            self.render_icon_modal(ctx);
        }
    }
}

impl UnitraySettingsApp {
    fn render_monitored_tab(&mut self, ui: &mut egui::Ui) {
        ui.add_space(4.0);
        ui.horizontal(|ui| {
            if ui.button("➕ Add Custom Service").clicked() {
                self.add_modal_open = true;
                self.custom_unit_input.clear();
                self.custom_name_input.clear();
            }
            if ui.button("⬆ Move Up").clicked() {
                self.move_selected_service(-1);
            }
            if ui.button("⬇ Move Down").clicked() {
                self.move_selected_service(1);
            }
            if ui.button("🗑 Remove").clicked() {
                self.remove_selected_service();
            }

            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.label("💡 Tip: You can edit Display Names directly in the list.");
            });
        });
        ui.separator();

        egui::ScrollArea::vertical().show(ui, |ui| {
            egui::Grid::new("monitored_table")
                .striped(true)
                .min_col_width(80.0)
                .spacing([12.0, 8.0])
                .show(ui, |ui| {
                    // Headers
                    ui.strong("#");
                    ui.strong("Display Name");
                    ui.strong("Systemd Service Unit");
                    ui.strong("Status");
                    ui.strong("Order");
                    ui.end_row();

                    let mut swap_action: Option<(usize, usize)> = None;
                    let mut remove_action: Option<usize> = None;
                    let services_len = self.config.services.len();
                    let statuses: Vec<bool> = self
                        .config
                        .services
                        .iter()
                        .map(|s| self.is_service_active(&s.unit))
                        .collect();

                    for (idx, entry) in self.config.services.iter_mut().enumerate() {
                        let is_selected = self.selected_monitored_idx == Some(idx);
                        let is_active = statuses.get(idx).copied().unwrap_or(false);

                        // Row selection indicator
                        if ui.selectable_label(is_selected, format!("{}", idx + 1)).clicked() {
                            self.selected_monitored_idx = Some(idx);
                        }

                        // Editable display name
                        ui.text_edit_singleline(&mut entry.name);

                        // Unit name
                        ui.label(&entry.unit);

                        // Status badge
                        if is_active {
                            ui.colored_label(egui::Color32::from_rgb(46, 204, 113), "🟢 Active");
                        } else {
                            ui.colored_label(egui::Color32::from_rgb(231, 76, 60), "🔴 Inactive");
                        }

                        // Inline Order Actions
                        ui.horizontal(|ui| {
                            if idx > 0 && ui.small_button("▲").clicked() {
                                swap_action = Some((idx, idx - 1));
                            }
                            if idx + 1 < services_len && ui.small_button("▼").clicked() {
                                swap_action = Some((idx, idx + 1));
                            }
                            if ui.small_button("✖").clicked() {
                                remove_action = Some(idx);
                            }
                        });

                        ui.end_row();
                    }

                    if let Some((a, b)) = swap_action {
                        self.config.services.swap(a, b);
                        self.selected_monitored_idx = Some(b);
                    }
                    if let Some(idx) = remove_action {
                        self.config.services.remove(idx);
                        self.selected_monitored_idx = None;
                    }
                });
        });
    }

    fn render_explorer_tab(&mut self, ui: &mut egui::Ui) {
        ui.add_space(4.0);
        ui.horizontal(|ui| {
            ui.label("🔍 Search:");
            ui.text_edit_singleline(&mut self.service_filter);
            if ui.button("Clear").clicked() {
                self.service_filter.clear();
            }

            let filter_lower = self.service_filter.trim().to_lowercase();
            let count = self
                .all_services
                .iter()
                .filter(|s| {
                    filter_lower.is_empty()
                        || s.unit.to_lowercase().contains(&filter_lower)
                        || s.description.to_lowercase().contains(&filter_lower)
                })
                .count();

            ui.label(format!("Showing {} of {} services", count, self.all_services.len()));
        });
        ui.separator();

        let monitored_units: HashSet<String> =
            self.config.services.iter().map(|s| s.unit.clone()).collect();
        let filter_lower = self.service_filter.trim().to_lowercase();

        let mut toggle_enable: Option<String> = None;
        let mut toggle_disable: Option<String> = None;

        egui::ScrollArea::vertical().show(ui, |ui| {
            egui::Grid::new("explorer_table")
                .striped(true)
                .min_col_width(60.0)
                .spacing([12.0, 6.0])
                .show(ui, |ui| {
                    // Headers
                    ui.strong("Monitor");
                    ui.strong("Service Unit");
                    ui.strong("Status");
                    ui.strong("Description");
                    ui.end_row();

                    for service in &self.all_services {
                        if !filter_lower.is_empty()
                            && !service.unit.to_lowercase().contains(&filter_lower)
                            && !service.description.to_lowercase().contains(&filter_lower)
                        {
                            continue;
                        }

                        let is_monitored = monitored_units.contains(&service.unit);
                        let mut checked = is_monitored;

                        if ui.checkbox(&mut checked, "").changed() {
                            if checked {
                                toggle_enable = Some(service.unit.clone());
                            } else {
                                toggle_disable = Some(service.unit.clone());
                            }
                        }

                        ui.label(&service.unit);

                        if service.is_active {
                            ui.colored_label(egui::Color32::from_rgb(46, 204, 113), "🟢 Active");
                        } else {
                            ui.colored_label(egui::Color32::from_rgb(231, 76, 60), "🔴 Inactive");
                        }

                        ui.label(&service.description);
                        ui.end_row();
                    }
                });
        });

        if let Some(unit) = toggle_enable {
            let display_name = format_display_name(&unit);
            self.config.services.push(ServiceEntry {
                name: display_name,
                unit,
            });
        }
        if let Some(unit) = toggle_disable {
            self.config.services.retain(|s| s.unit != unit);
        }
    }

    fn render_general_tab(&mut self, ui: &mut egui::Ui) {
        ui.add_space(8.0);
        egui::Grid::new("general_options_grid")
            .num_columns(2)
            .spacing([16.0, 14.0])
            .show(ui, |ui| {
                ui.strong("Refresh Polling Interval:");
                ui.horizontal(|ui| {
                    ui.add(
                        egui::Slider::new(&mut self.config.refresh_interval_secs, 1..=60)
                            .text("seconds"),
                    );
                });
                ui.end_row();

                ui.strong("Tray Icon Preset:");
                let current_icon = self.config.tray_icon.clone();
                let current_label = POPULAR_TRAY_ICONS
                    .iter()
                    .find(|&&(id, _)| id == current_icon)
                    .map(|&(_, label)| label)
                    .unwrap_or("Custom Icon");

                egui::ComboBox::from_id_salt("icon_combo")
                    .selected_text(current_label)
                    .show_ui(ui, |ui| {
                        for &(id, label) in POPULAR_TRAY_ICONS {
                            if ui
                                .selectable_value(&mut self.config.tray_icon, id.to_string(), label)
                                .clicked()
                            {}
                        }
                    });
                ui.end_row();

                ui.strong("Custom Icon Name:");
                ui.horizontal(|ui| {
                    ui.text_edit_singleline(&mut self.config.tray_icon);
                    if ui.button("🔍 Browse System Icons...").clicked() {
                        self.icon_modal_open = true;
                        self.icon_search_filter.clear();
                    }
                });
                ui.end_row();

                ui.strong("Configuration File:");
                ui.horizontal(|ui| {
                    if ui.button("📂 Open in External Text Editor").clicked() {
                        if let Ok(path) = config::get_user_config_path() {
                            let _ = Command::new("xdg-open").arg(path).spawn();
                        }
                    }
                });
                ui.end_row();
            });
    }

    fn render_add_modal(&mut self, ctx: &egui::Context) {
        egui::Window::new("Add Custom Systemd Service")
            .collapsible(false)
            .resizable(false)
            .fixed_size([480.0, 360.0])
            .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0])
            .show(ctx, |ui| {
                ui.label("Enter systemd unit name (e.g. redis-server.service, nginx.service):");
                let unit_edit = ui.text_edit_singleline(&mut self.custom_unit_input);
                if unit_edit.changed() && self.custom_name_input.is_empty() {
                    self.custom_name_input = format_display_name(&self.custom_unit_input);
                }

                // Autocomplete suggestion list
                let search_term = self.custom_unit_input.trim().to_lowercase();
                if !search_term.is_empty() {
                    ui.add_space(4.0);
                    ui.label("Suggested Services (click to fill):");
                    egui::ScrollArea::vertical()
                        .max_height(140.0)
                        .show(ui, |ui| {
                            let matches: Vec<&SystemdServiceInfo> = self
                                .all_services
                                .iter()
                                .filter(|s| s.unit.to_lowercase().contains(&search_term))
                                .take(8)
                                .collect();

                            for s in matches {
                                if ui
                                    .button(format!("{} - {}", s.unit, s.description))
                                    .clicked()
                                {
                                    self.custom_unit_input = s.unit.clone();
                                    self.custom_name_input = format_display_name(&s.unit);
                                }
                            }
                        });
                }

                ui.add_space(8.0);
                ui.label("Display Name in Menu:");
                ui.text_edit_singleline(&mut self.custom_name_input);

                ui.add_space(12.0);
                ui.horizontal(|ui| {
                    let can_add = !self.custom_unit_input.trim().is_empty();
                    if ui
                        .add_enabled(can_add, egui::Button::new("➕ Add Service"))
                        .clicked()
                    {
                        let unit = self.custom_unit_input.trim().to_string();
                        let name = if self.custom_name_input.trim().is_empty() {
                            format_display_name(&unit)
                        } else {
                            self.custom_name_input.trim().to_string()
                        };

                        if !self.config.services.iter().any(|s| s.unit == unit) {
                            self.config.services.push(ServiceEntry { name, unit });
                        }
                        self.add_modal_open = false;
                    }

                    if ui.button("Cancel").clicked() {
                        self.add_modal_open = false;
                    }
                });
            });
    }

    fn render_icon_modal(&mut self, ctx: &egui::Context) {
        egui::Window::new("Select System Tray Icon")
            .collapsible(false)
            .resizable(true)
            .default_size([560.0, 420.0])
            .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0])
            .show(ctx, |ui| {
                ui.horizontal(|ui| {
                    ui.label("🔍 Filter:");
                    ui.text_edit_singleline(&mut self.icon_search_filter);
                    if ui.button("Clear").clicked() {
                        self.icon_search_filter.clear();
                    }
                });
                ui.separator();

                let search_lower = self.icon_search_filter.trim().to_lowercase();
                let mut picked: Option<String> = None;

                egui::ScrollArea::vertical().show(ui, |ui| {
                    egui::Grid::new("icon_grid")
                        .striped(true)
                        .num_columns(2)
                        .spacing([12.0, 6.0])
                        .show(ui, |ui| {
                            let filtered = self
                                .discovered_icons
                                .iter()
                                .filter(|name| {
                                    search_lower.is_empty()
                                        || name.to_lowercase().contains(&search_lower)
                                });

                            for icon_name in filtered.take(200) {
                                ui.label(icon_name);
                                if ui.button("Select").clicked() {
                                    picked = Some(icon_name.clone());
                                }
                                ui.end_row();
                            }
                        });
                });

                if let Some(chosen) = picked {
                    self.config.tray_icon = chosen;
                    self.icon_modal_open = false;
                }

                ui.add_space(8.0);
                if ui.button("Close").clicked() {
                    self.icon_modal_open = false;
                }
            });
    }
}

fn format_display_name(unit: &str) -> String {
    let clean = unit
        .trim_end_matches(".service")
        .replace('-', " ")
        .replace('_', " ");

    clean
        .split_whitespace()
        .map(|word| {
            let mut c = word.chars();
            match c.next() {
                None => String::new(),
                Some(f) => f.to_uppercase().collect::<String>() + c.as_str(),
            }
        })
        .collect::<Vec<_>>()
        .join(" ")
}

fn discover_system_icons() -> Vec<String> {
    let mut icons = HashSet::new();
    icons.insert("unitray".to_string());

    let dirs = [
        PathBuf::from("/usr/share/icons/breeze"),
        PathBuf::from("/usr/share/icons/hicolor"),
        dirs::data_dir().unwrap_or_default().join("icons/hicolor"),
    ];

    for dir in &dirs {
        if dir.exists() {
            scan_icons_dir(dir, &mut icons, 0);
        }
    }

    let mut list: Vec<String> = icons.into_iter().collect();
    list.sort();
    list
}

fn scan_icons_dir(dir: &Path, set: &mut HashSet<String>, depth: usize) {
    if depth > 4 {
        return;
    }
    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                scan_icons_dir(&path, set, depth + 1);
            } else if let Some(ext) = path.extension().and_then(|s| s.to_str()) {
                if ext == "png" || ext == "svg" {
                    if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                        if !stem.ends_with("-symbolic") && !stem.starts_with("scalable") {
                            set.insert(stem.to_string());
                        }
                    }
                }
            }
        }
    }
}

fn main() -> Result<(), eframe::Error> {
    // Single instance enforcement
    let guard = match lock::acquire_named_lock("unitray-settings.lock") {
        Ok(guard) => guard,
        Err(_) => {
            println!("Unitray Settings is already open. Exiting.");
            return Ok(());
        }
    };

    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_title("Unitray Settings")
            .with_inner_size([780.0, 560.0])
            .with_min_inner_size([540.0, 380.0]),
        ..Default::default()
    };

    eframe::run_native(
        "Unitray Settings",
        options,
        Box::new(|cc| {
            // Apply Breeze-inspired dark visuals
            let mut visuals = egui::Visuals::dark();
            visuals.window_rounding = 8.0.into();
            visuals.window_shadow.spread = 8.0;
            visuals.widgets.noninteractive.bg_fill = egui::Color32::from_rgb(28, 31, 38);
            visuals.widgets.inactive.bg_fill = egui::Color32::from_rgb(38, 43, 52);
            visuals.widgets.hovered.bg_fill = egui::Color32::from_rgb(52, 60, 74);
            visuals.widgets.active.bg_fill = egui::Color32::from_rgb(61, 174, 233);
            visuals.selection.bg_fill = egui::Color32::from_rgb(61, 174, 233);
            cc.egui_ctx.set_visuals(visuals);

            Ok(Box::new(UnitraySettingsApp::new(guard)))
        }),
    )
}
