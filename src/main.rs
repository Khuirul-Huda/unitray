mod config;
mod lock;
mod service;
mod tray;

use ksni::blocking::TrayMethods;
use std::thread;
use std::time::Duration;
use tray::UnitrayItem;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() > 1 {
        match args[1].as_str() {
            "-h" | "--help" => {
                println!("Unitray - Systemd service tray manager");
                println!();
                println!("Usage: unitray [OPTIONS]");
                println!();
                println!("Options:");
                println!("  -h, --help       Print help information");
                println!("  -v, --version    Print version information");
                println!("  --settings       Open graphical settings window");
                return;
            }
            "-v" | "--version" => {
                println!("unitray {}", env!("CARGO_PKG_VERSION"));
                return;
            }
            "--settings" => {
                if let Err(e) = std::process::Command::new("unitray-settings").spawn() {
                    eprintln!("Failed to launch unitray-settings: {}", e);
                }
                return;
            }
            _ => {}
        }
    }

    // Ensure single instance to prevent duplicate tray icons
    let _instance_guard = match lock::acquire_single_instance_lock() {
        Ok(guard) => guard,
        Err(existing_pid) => {
            if let Some(pid) = existing_pid {
                println!("Unitray is already running (PID {}). Exiting.", pid);
            } else {
                println!("Unitray is already running. Exiting.");
            }
            return;
        }
    };

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
