#pragma once

#include <QString>

class Autostart {
public:
    static bool isEnabled();
    static bool setEnabled(bool enable);
    static QString autostartFilePath();

private:
    static bool writeAutostartFile(const QString &filePath);
    static bool writeDisabledDesktopFile(const QString &filePath);
};
