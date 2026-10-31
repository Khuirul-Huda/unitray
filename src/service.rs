use std::collections::HashMap;
use std::process::Command;

const SYSTEMCTL_BINARY: &str = "systemctl";
const ACTIVE_STATE_STRING: &str = "active";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SystemdServiceInfo {
    pub unit: String,
    pub is_active: bool,
    pub description: String,
}

pub fn query_all_systemd_services() -> Vec<SystemdServiceInfo> {
    let mut services: HashMap<String, (bool, String)> = HashMap::new();

    // 1. Query all service unit files
    if let Ok(output) = Command::new(SYSTEMCTL_BINARY)
        .args(["list-unit-files", "--type=service", "--no-legend", "--no-pager"])
        .output()
    {
        let stdout = String::from_utf8_lossy(&output.stdout);
        for line in stdout.lines() {
            let mut parts = line.split_whitespace();
            if let Some(unit) = parts.next() {
                if unit.ends_with(".service") {
                    services.insert(unit.to_string(), (false, String::new()));
                }
            }
        }
    }

    // 2. Query runtime states and descriptions
    if let Ok(output) = Command::new(SYSTEMCTL_BINARY)
        .args(["list-units", "--type=service", "--all", "--no-legend", "--no-pager"])
        .output()
    {
        let stdout = String::from_utf8_lossy(&output.stdout);
        for line in stdout.lines() {
            let mut parts = line.split_whitespace();
            if let (Some(unit), Some(_load), Some(active_state), Some(_sub)) =
                (parts.next(), parts.next(), parts.next(), parts.next())
            {
                let description = parts.collect::<Vec<_>>().join(" ");
                let is_active = active_state == ACTIVE_STATE_STRING;
                if let Some(entry) = services.get_mut(unit) {
                    entry.0 = is_active;
                    if !description.is_empty() {
                        entry.1 = description;
                    }
                } else if unit.ends_with(".service") {
                    services.insert(unit.to_string(), (is_active, description));
                }
            }
        }
    }

    let mut result: Vec<SystemdServiceInfo> = services
        .into_iter()
        .map(|(unit, (is_active, description))| SystemdServiceInfo {
            unit,
            is_active,
            description,
        })
        .collect();

    result.sort_by(|a, b| a.unit.cmp(&b.unit));
    result
}

pub fn is_service_active(unit_name: &str) -> bool {
    let output = Command::new(SYSTEMCTL_BINARY)
        .arg("is-active")
        .arg(unit_name)
        .output();

    match output {
        Ok(out) => {
            let status = String::from_utf8_lossy(&out.stdout);
            status.trim() == ACTIVE_STATE_STRING
        }
        Err(_) => false,
    }
}

pub fn start_service(unit_name: &str) -> Result<(), String> {
    execute_systemctl_command("start", unit_name)
}

pub fn stop_service(unit_name: &str) -> Result<(), String> {
    execute_systemctl_command("stop", unit_name)
}

pub fn restart_service(unit_name: &str) -> Result<(), String> {
    execute_systemctl_command("restart", unit_name)
}

fn execute_systemctl_command(action: &str, unit_name: &str) -> Result<(), String> {
    let status = Command::new(SYSTEMCTL_BINARY)
        .arg(action)
        .arg(unit_name)
        .status();

    match status {
        Ok(exit_status) if exit_status.success() => Ok(()),
        Ok(exit_status) => Err(format!(
            "Failed to {} {}: exit code {:?}",
            action, unit_name, exit_status.code()
        )),
        Err(err) => Err(format!("Failed to execute systemctl: {}", err)),
    }
}
