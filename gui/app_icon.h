#pragma once

#include <QIcon>
#include <QDir>
#include <QFile>

inline QIcon resolveAppIcon() {
    QIcon icon = QIcon::fromTheme("unitray");
    if (!icon.isNull()) {
        return icon;
    }

    const QString homePix = QDir::homePath() + "/.local/share/pixmaps/unitray.png";
    if (QFile::exists(homePix)) {
        return QIcon(homePix);
    }

    if (QFile::exists("/usr/share/pixmaps/unitray.png")) {
        return QIcon("/usr/share/pixmaps/unitray.png");
    }

    return QIcon::fromTheme("preferences-system-services");
}
