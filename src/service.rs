use std::process::Command;

const SYSTEMCTL_BINARY: &str = "systemctl";
const ACTIVE_STATE_STRING: &str = "active";

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
