use std::fs::{File, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
use std::os::unix::io::AsRawFd;
use std::path::PathBuf;

/// RAII guard that holds the flock lock for the lifetime of the application.
pub struct SingleInstanceGuard {
    _file: File,
}

/// Resolves the user-specific path for the lock file.
fn resolve_lock_path() -> PathBuf {
    if let Some(runtime_dir) = dirs::runtime_dir() {
        runtime_dir.join("unitray.lock")
    } else {
        let uid = unsafe { libc::getuid() };
        std::env::temp_dir().join(format!("unitray-{}.lock", uid))
    }
}

/// Attempts to acquire an exclusive lock to ensure only a single instance of Unitray runs.
/// Returns Ok(SingleInstanceGuard) if this is the only instance.
/// Returns Err(Some(pid)) or Err(None) if another instance is already running.
pub fn acquire_single_instance_lock() -> Result<SingleInstanceGuard, Option<u32>> {
    let lock_path = resolve_lock_path();

    let mut file = OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(false)
        .open(&lock_path)
        .map_err(|e| {
            eprintln!("Warning: Failed to open lockfile {:?}: {}", lock_path, e);
            None
        })?;

    let fd = file.as_raw_fd();
    let lock_result = unsafe { libc::flock(fd, libc::LOCK_EX | libc::LOCK_NB) };

    if lock_result != 0 {
        let err = std::io::Error::last_os_error();
        if err.raw_os_error() == Some(libc::EWOULDBLOCK) || err.raw_os_error() == Some(libc::EAGAIN) {
            let mut pid_buf = String::new();
            let _ = file.read_to_string(&mut pid_buf);
            let existing_pid = pid_buf.trim().parse::<u32>().ok();
            return Err(existing_pid);
        }
        return Err(None);
    }

    // Acquired exclusive lock; record current PID
    let _ = file.set_len(0);
    let _ = file.seek(SeekFrom::Start(0));
    let _ = writeln!(file, "{}", std::process::id());
    let _ = file.flush();

    Ok(SingleInstanceGuard { _file: file })
}
