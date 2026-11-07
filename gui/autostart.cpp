#include "autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

namespace {
constexpr const char *SYSTEM_AUTOSTART_PATH = "/etc/xdg/autostart/unitray.desktop";
constexpr const char *AUTOSTART_FILE_NAME = "unitray.desktop";

bool ensureParentDirectoryExists(const QString &filePath) {
    const QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(".");
}
} // namespace

QString Autostart::autostartFilePath() {
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configDir + "/autostart/" + AUTOSTART_FILE_NAME;
}

bool Autostart::isEnabled() {
    const QString userPath = autostartFilePath();
    if (QFile::exists(userPath)) {
        QFile file(userPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                if (line.compare("Hidden=true", Qt::CaseInsensitive) == 0 ||
                    line.compare("X-GNOME-Autostart-enabled=false", Qt::CaseInsensitive) == 0) {
                    return false;
                }
            }
            return true;
        }
    }

    if (QFile::exists(SYSTEM_AUTOSTART_PATH)) {
        return true;
    }

    return false;
}

bool Autostart::setEnabled(bool enable) {
    const QString targetPath = autostartFilePath();
    if (enable) {
        return writeAutostartFile(targetPath);
    }
    return writeDisabledDesktopFile(targetPath);
}

bool Autostart::writeAutostartFile(const QString &filePath) {
    if (!ensureParentDirectoryExists(filePath)) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=Unitray\n";
    out << "GenericName=Service Tray Manager\n";
    out << "Comment=Manage developer systemd services from the system tray\n";
    out << "Exec=unitray\n";
    out << "Icon=unitray\n";
    out << "Terminal=false\n";
    out << "Categories=Development;System;Utility;\n";
    out << "StartupNotify=false\n";
    out << "X-GNOME-Autostart-enabled=true\n";
    out << "X-KDE-autostart-phase=2\n";
    return true;
}

bool Autostart::writeDisabledDesktopFile(const QString &filePath) {
    if (QFile::exists(SYSTEM_AUTOSTART_PATH)) {
        if (!ensureParentDirectoryExists(filePath)) {
            return false;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }

        QTextStream out(&file);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=Unitray\n";
        out << "Exec=unitray\n";
        out << "Hidden=true\n";
        out << "X-GNOME-Autostart-enabled=false\n";
        return true;
    }

    if (QFile::exists(filePath)) {
        return QFile::remove(filePath);
    }

    return true;
}
