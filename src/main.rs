mod config;
mod service;
mod tray;

use ksni::blocking::TrayMethods;
use std::thread;
use std::time::Duration;
use tray::UnitrayItem;

fn main() {
    let config = config::load_configuration();
    let refresh_interval = Duration::from_secs(config.refresh_interval_secs);

    let tray_item = UnitrayItem::from_config(&config);
    let handle = tray_item
        .spawn()
        .expect("Failed to spawn StatusNotifierItem tray service");

    loop {
        thread::sleep(refresh_interval);
        handle.update(|item: &mut UnitrayItem| {
            item.refresh_states();
        });
    }
}
