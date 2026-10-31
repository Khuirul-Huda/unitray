#include "settings_window.h"

#include <QApplication>
#include <QStandardPaths>
#include <QIcon>
#include <iostream>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

static int s_lockFd = -1;

static bool acquireSingleInstanceLock() {
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    QString lockPath = runtimeDir + "/unitray-settings.lock";

    s_lockFd = open(lockPath.toUtf8().constData(), O_RDWR | O_CREAT, 0600);
    if (s_lockFd < 0) {
        return false;
    }

    if (flock(s_lockFd, LOCK_EX | LOCK_NB) != 0) {
        close(s_lockFd);
        s_lockFd = -1;
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (!acquireSingleInstanceLock()) {
        std::cout << "Unitray Settings is already open. Exiting." << std::endl;
        return 0;
    }

    QApplication app(argc, argv);
    app.setApplicationName("unitray-settings");
    app.setApplicationDisplayName("Unitray Settings");
    app.setWindowIcon(QIcon::fromTheme("unitray", QIcon::fromTheme("preferences-system-services")));

    SettingsWindow window;
    window.show();

    int exitCode = app.exec();

    if (s_lockFd >= 0) {
        flock(s_lockFd, LOCK_UN);
        close(s_lockFd);
        s_lockFd = -1;
    }

    return exitCode;
}
