#include "settings_window.h"
#include "about_dialog.h"
#include "app_icon.h"

#include <QApplication>
#include <QStandardPaths>
#include <iostream>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

static int s_lockFd = -1;

static bool acquireSingleInstanceLock(const QString &lockName) {
    QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    const QString lockPath = runtimeDir + "/" + lockName + ".lock";

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

static void releaseSingleInstanceLock() {
    if (s_lockFd >= 0) {
        flock(s_lockFd, LOCK_UN);
        close(s_lockFd);
        s_lockFd = -1;
    }
}

static bool isAboutArgument(const QString &arg) {
    return arg == "--about" || arg == "-a" || arg == "about";
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("unitray-settings");
    app.setApplicationDisplayName("Unitray Settings");
    app.setWindowIcon(resolveAppIcon());

    bool showAbout = false;
    for (int i = 1; i < argc; ++i) {
        if (isAboutArgument(QString::fromUtf8(argv[i]))) {
            showAbout = true;
            break;
        }
    }

    if (showAbout) {
        if (!acquireSingleInstanceLock("unitray-about")) {
            std::cout << "Unitray About is already open." << std::endl;
            return 0;
        }

        AboutDialog dialog;
        const int exitCode = dialog.exec();
        releaseSingleInstanceLock();
        return exitCode;
    }

    if (!acquireSingleInstanceLock("unitray-settings")) {
        std::cout << "Unitray Settings is already open. Exiting." << std::endl;
        return 0;
    }

    SettingsWindow window;
    window.show();

    const int exitCode = app.exec();
    releaseSingleInstanceLock();
    return exitCode;
}
