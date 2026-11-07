#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QListWidget>
#include <QCompleter>
#include <QStringList>
#include <QVector>
#include <QMap>

struct MonitoredService {
    QString name;
    QString unit;
};

struct AppConfig {
    int refreshIntervalSecs = 3;
    QString trayIcon = "unitray";
    bool autostart = false;
    QVector<MonitoredService> services;
};

struct SystemdServiceInfo {
    QString unit;
    bool isActive = false;
    QString description;
};

class IconPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit IconPickerDialog(const QStringList &icons, const QString &currentIcon, QWidget *parent = nullptr);
    QString selectedIcon() const { return m_selectedIcon; }

private slots:
    void onFilterChanged(const QString &text);
    void onItemClicked(QListWidgetItem *item);
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    QLineEdit *m_filterEdit;
    QListWidget *m_listWidget;
    QLabel *m_previewLabel;
    QLabel *m_nameLabel;
    QString m_selectedIcon;
    QStringList m_allIcons;
};

class AddCustomServiceDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddCustomServiceDialog(const QStringList &availableUnits, QWidget *parent = nullptr);
    QString unitName() const;
    QString displayName() const;

private slots:
    void onUnitTextChanged(const QString &text);

private:
    QLineEdit *m_unitEdit;
    QLineEdit *m_nameEdit;
    QCompleter *m_completer;
};

class SettingsWindow : public QDialog {
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = nullptr);
    ~SettingsWindow() override = default;
    static QString formatDisplayName(const QString &unit);

private slots:
    void onAddCustomService();
    void onMoveUp();
    void onMoveDown();
    void onRemoveSelected();
    void onEditName();
    void onExplorerSearchChanged(const QString &text);
    void onExplorerCellChanged(int row, int column);
    void onPresetIconChanged(int index);
    void onCustomIconTextChanged(const QString &text);
    void onBrowseSystemIcons();
    void onOpenConfigFile();
    void onShowAbout();
    void onSaveAndApply();

private:
    void setupUi();
    void loadConfig();
    bool saveConfig();
    void querySystemdServices();
    void discoverSystemIcons();
    void populateMonitoredTable();
    void populateExplorerTable();
    void updateExplorerCheckboxes();
    static QString configFilePath();

    AppConfig m_config;
    QMap<QString, SystemdServiceInfo> m_allServices;
    QStringList m_discoveredIcons;

    QTabWidget *m_tabWidget;

    // Tab 1: Monitored
    QTableWidget *m_monitoredTable;
    QPushButton *m_btnAddCustom;
    QPushButton *m_btnMoveUp;
    QPushButton *m_btnMoveDown;
    QPushButton *m_btnEditName;
    QPushButton *m_btnRemove;

    // Tab 2: Explorer
    QLineEdit *m_explorerSearch;
    QTableWidget *m_explorerTable;
    QLabel *m_lblExplorerCount;
    bool m_updatingExplorer = false;

    // Tab 3: General
    QSpinBox *m_spinInterval;
    QComboBox *m_comboPresetIcons;
    QLineEdit *m_editCustomIcon;
    QLabel *m_lblIconPreview;
    QPushButton *m_btnBrowseIcons;
    QCheckBox *m_chkAutostart;
    QPushButton *m_btnOpenConfig;

    // Bottom bar
    QLabel *m_lblStatus;
    QPushButton *m_btnAbout;
    QPushButton *m_btnSave;
    QPushButton *m_btnCancel;
};
