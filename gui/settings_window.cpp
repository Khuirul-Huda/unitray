#include "settings_window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QIcon>
#include <QStandardPaths>
#include <QRegularExpression>

static const struct {
    const char *id;
    const char *label;
} POPULAR_ICONS[] = {
    {"unitray", "Unitray 3D Clay Logo"},
    {"preferences-system-services", "System Services (Gears)"},
    {"server-database", "Database Server"},
    {"system-run", "System Run"},
    {"utilities-system-monitor", "System Monitor"},
    {"network-server", "Network Server"},
    {"applications-system", "System Applications"},
    {"systemsettings", "System Settings"},
    {"cpu", "CPU / Processor"},
    {"dialog-memory", "Memory / RAM"},
    {"internet-services", "Internet Services"},
    {"network-wired", "Network Wired"},
    {nullptr, nullptr}
};

// ==================== IconPickerDialog ====================

IconPickerDialog::IconPickerDialog(const QStringList &icons, const QString &currentIcon, QWidget *parent)
    : QDialog(parent), m_selectedIcon(currentIcon), m_allIcons(icons)
{
    setWindowTitle(tr("Select System Tray Icon"));
    setWindowIcon(QIcon::fromTheme("preferences-system-services"));
    resize(720, 520);

    auto *layout = new QVBoxLayout(this);

    auto *searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel(tr("🔍 Filter Icons:")));
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText(tr("Search by keyword (e.g. server, gear, service, db, run, cpu)..."));
    m_filterEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_filterEdit);
    layout->addLayout(searchLayout);

    m_listWidget = new QListWidget();
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setIconSize(QSize(36, 36));
    m_listWidget->setGridSize(QSize(120, 80));
    m_listWidget->setMovement(QListWidget::Static);
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setWordWrap(true);
    layout->addWidget(m_listWidget);

    auto *bottomLayout = new QHBoxLayout();
    m_previewLabel = new QLabel();
    m_previewLabel->setFixedSize(32, 32);
    bottomLayout->addWidget(m_previewLabel);

    m_nameLabel = new QLabel(QString("Selected: <b>%1</b>").arg(currentIcon));
    m_nameLabel->setTextFormat(Qt::RichText);
    bottomLayout->addWidget(m_nameLabel);
    bottomLayout->addStretch();

    auto *btnCancel = new QPushButton(tr("Cancel"));
    auto *btnChoose = new QPushButton(tr("Select Icon"));
    btnChoose->setDefault(true);
    bottomLayout->addWidget(btnCancel);
    bottomLayout->addWidget(btnChoose);
    layout->addLayout(bottomLayout);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &IconPickerDialog::onFilterChanged);
    connect(m_listWidget, &QListWidget::itemClicked, this, &IconPickerDialog::onItemClicked);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &IconPickerDialog::onItemDoubleClicked);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnChoose, &QPushButton::clicked, this, &QDialog::accept);

    onFilterChanged("");
}

void IconPickerDialog::onFilterChanged(const QString &text) {
    m_listWidget->clear();
    QString query = text.trimmed().toLower();
    int count = 0;

    for (const QString &iconName : m_allIcons) {
        if (!query.isEmpty() && !iconName.toLower().contains(query)) {
            continue;
        }

        QIcon icon = QIcon::fromTheme(iconName);
        if (!icon.isNull()) {
            auto *item = new QListWidgetItem(icon, iconName);
            item->setData(Qt::UserRole, iconName);
            m_listWidget->addItem(item);
            if (++count >= 300) break;
        }
    }
}

void IconPickerDialog::onItemClicked(QListWidgetItem *item) {
    if (!item) return;
    m_selectedIcon = item->data(Qt::UserRole).toString();
    m_nameLabel->setText(QString("Selected: <b>%1</b>").arg(m_selectedIcon));
    QIcon icon = QIcon::fromTheme(m_selectedIcon);
    if (!icon.isNull()) {
        m_previewLabel->setPixmap(icon.pixmap(32, 32));
    }
}

void IconPickerDialog::onItemDoubleClicked(QListWidgetItem *item) {
    onItemClicked(item);
    accept();
}

// ==================== AddCustomServiceDialog ====================

AddCustomServiceDialog::AddCustomServiceDialog(const QStringList &availableUnits, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Custom Systemd Service"));
    setWindowIcon(QIcon::fromTheme("list-add"));
    resize(480, 220);

    auto *layout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();

    m_unitEdit = new QLineEdit();
    m_unitEdit->setPlaceholderText(tr("e.g. redis-server.service, nginx.service"));
    m_unitEdit->setClearButtonEnabled(true);

    m_completer = new QCompleter(availableUnits, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setMaxVisibleItems(10);
    m_unitEdit->setCompleter(m_completer);

    formLayout->addRow(tr("Systemd Unit Name:"), m_unitEdit);

    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("e.g. Redis Server, Nginx"));
    m_nameEdit->setClearButtonEnabled(true);
    formLayout->addRow(tr("Display Name in Menu:"), m_nameEdit);

    layout->addLayout(formLayout);

    auto *hintLabel = new QLabel(tr("💡 Start typing the service unit name to see autocomplete suggestions."));
    hintLabel->setStyleSheet("color: gray; font-size: 11px;");
    layout->addWidget(hintLabel);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *btnCancel = new QPushButton(tr("Cancel"));
    auto *btnAdd = new QPushButton(tr("➕ Add Service"));
    btnAdd->setDefault(true);
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnAdd);
    layout->addLayout(btnLayout);

    connect(m_unitEdit, &QLineEdit::textChanged, this, &AddCustomServiceDialog::onUnitTextChanged);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnAdd, &QPushButton::clicked, this, [this]() {
        if (m_unitEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Warning"), tr("Please enter a valid systemd service unit name."));
            return;
        }
        accept();
    });
}

QString AddCustomServiceDialog::unitName() const {
    return m_unitEdit->text().trimmed();
}

QString AddCustomServiceDialog::displayName() const {
    QString name = m_nameEdit->text().trimmed();
    return name.isEmpty() ? SettingsWindow::formatDisplayName(unitName()) : name;
}

void AddCustomServiceDialog::onUnitTextChanged(const QString &text) {
    if (m_nameEdit->text().isEmpty() || m_nameEdit->property("auto").toBool()) {
        m_nameEdit->setText(SettingsWindow::formatDisplayName(text));
        m_nameEdit->setProperty("auto", true);
    }
}

// ==================== SettingsWindow ====================

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Unitray Settings"));
    QIcon appIcon = QIcon::fromTheme("unitray");
    if (appIcon.isNull()) {
        QString homePix = QDir::homePath() + "/.local/share/pixmaps/unitray.png";
        if (QFile::exists(homePix)) appIcon = QIcon(homePix);
        else appIcon = QIcon::fromTheme("preferences-system-services");
    }
    setWindowIcon(appIcon);
    resize(780, 560);
    setMinimumSize(600, 420);

    loadConfig();
    querySystemdServices();
    discoverSystemIcons();
    setupUi();

    populateMonitoredTable();
    populateExplorerTable();
}

QString SettingsWindow::configFilePath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/unitray";
    return configDir + "/config.toml";
}

QString SettingsWindow::formatDisplayName(const QString &unit) {
    QString clean = unit;
    if (clean.endsWith(".service")) {
        clean.chop(8);
    }
    clean.replace("-", " ").replace("_", " ");

    QStringList words = clean.split(" ", Qt::SkipEmptyParts);
    for (QString &w : words) {
        if (!w.isEmpty()) {
            w[0] = w[0].toUpper();
        }
    }
    return words.join(" ");
}

void SettingsWindow::loadConfig() {
    m_config.refreshIntervalSecs = 3;
    m_config.trayIcon = "unitray";
    m_config.services.clear();

    QString path = configFilePath();
    QFile file(path);
    if (!file.exists()) {
        file.setFileName("/etc/unitray/config.toml");
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Default services
        m_config.services.append(MonitoredService{"MariaDB", "mariadb.service"});
        m_config.services.append(MonitoredService{"PostgreSQL", "postgresql.service"});
        m_config.services.append(MonitoredService{"Docker", "docker.service"});
        m_config.services.append(MonitoredService{"Cloudflare Tunnel", "cloudflared.service"});
        return;
    }

    QTextStream in(&file);
    MonitoredService currentService;
    bool inServiceBlock = false;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;

        if (line == "[[services]]") {
            if (inServiceBlock && !currentService.unit.isEmpty()) {
                m_config.services.append(currentService);
            }
            currentService = MonitoredService();
            inServiceBlock = true;
            continue;
        }

        if (line.contains("=")) {
            int eqIdx = line.indexOf("=");
            QString key = line.left(eqIdx).trimmed();
            QString val = line.mid(eqIdx + 1).trimmed();
            if (val.startsWith("\"") && val.endsWith("\"") && val.length() >= 2) {
                val = val.mid(1, val.length() - 2);
            }

            if (key == "refresh_interval_secs") {
                m_config.refreshIntervalSecs = val.toInt();
            } else if (key == "tray_icon") {
                m_config.trayIcon = val;
            } else if (inServiceBlock) {
                if (key == "name") currentService.name = val;
                else if (key == "unit") currentService.unit = val;
            }
        }
    }

    if (inServiceBlock && !currentService.unit.isEmpty()) {
        m_config.services.append(currentService);
    }
}

bool SettingsWindow::saveConfig() {
    QString path = configFilePath();
    QFileInfo fi(path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "# Unitray Configuration\n";
    out << "# Auto-generated by Unitray Settings\n\n";
    out << "refresh_interval_secs = " << m_config.refreshIntervalSecs << "\n";
    out << "tray_icon = \"" << m_config.trayIcon << "\"\n\n";

    for (const auto &s : m_config.services) {
        out << "[[services]]\n";
        out << "name = \"" << s.name << "\"\n";
        out << "unit = \"" << s.unit << "\"\n\n";
    }

    return true;
}

void SettingsWindow::querySystemdServices() {
    m_allServices.clear();

    // 1. List all available service unit files
    QProcess procFiles;
    procFiles.start("systemctl", {"list-unit-files", "--type=service", "--no-legend", "--no-pager"});
    if (procFiles.waitForFinished()) {
        QString output = procFiles.readAllStandardOutput();
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (!parts.isEmpty() && parts[0].endsWith(".service")) {
                SystemdServiceInfo info;
                info.unit = parts[0];
                info.isActive = false;
                m_allServices.insert(parts[0], info);
            }
        }
    }

    // 2. Query active states and descriptions
    QProcess procUnits;
    procUnits.start("systemctl", {"list-units", "--type=service", "--all", "--no-legend", "--no-pager"});
    if (procUnits.waitForFinished()) {
        QString output = procUnits.readAllStandardOutput();
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 4) {
                QString unit = parts[0];
                QString activeState = parts[2];
                QString desc = parts.mid(4).join(" ");
                bool isActive = (activeState == "active");

                if (m_allServices.contains(unit)) {
                    m_allServices[unit].isActive = isActive;
                    if (!desc.isEmpty()) m_allServices[unit].description = desc;
                } else if (unit.endsWith(".service")) {
                    SystemdServiceInfo info;
                    info.unit = unit;
                    info.isActive = isActive;
                    info.description = desc;
                    m_allServices.insert(unit, info);
                }
            }
        }
    }
}

void SettingsWindow::discoverSystemIcons() {
    m_discoveredIcons.clear();
    QSet<QString> iconSet;
    iconSet.insert("unitray");

    QStringList searchDirs = {
        "/usr/share/icons/breeze",
        "/usr/share/icons/hicolor",
        QDir::homePath() + "/.local/share/icons/hicolor"
    };

    for (const QString &dirPath : searchDirs) {
        if (!QDir(dirPath).exists()) continue;
        QDirIterator it(dirPath, QStringList() << "*.png" << "*.svg", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QString stem = it.fileInfo().baseName();
            if (!stem.endsWith("-symbolic") && !stem.startsWith("scalable")) {
                iconSet.insert(stem);
            }
        }
    }

    m_discoveredIcons = iconSet.values();
    m_discoveredIcons.sort();
}

void SettingsWindow::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget);

    // ==================== Tab 1: Monitored Services ====================
    auto *tab1 = new QWidget();
    auto *tab1Layout = new QVBoxLayout(tab1);

    auto *tab1Actions = new QHBoxLayout();
    m_btnAddCustom = new QPushButton(tr("➕ Add Custom Service"));
    m_btnMoveUp = new QPushButton(tr("⬆ Move Up"));
    m_btnMoveDown = new QPushButton(tr("⬇ Move Down"));
    m_btnEditName = new QPushButton(tr("✏ Edit Name"));
    m_btnRemove = new QPushButton(tr("🗑 Remove"));

    tab1Actions->addWidget(m_btnAddCustom);
    tab1Actions->addWidget(m_btnMoveUp);
    tab1Actions->addWidget(m_btnMoveDown);
    tab1Actions->addWidget(m_btnEditName);
    tab1Actions->addWidget(m_btnRemove);
    tab1Actions->addStretch();
    tab1Layout->addLayout(tab1Actions);

    m_monitoredTable = new QTableWidget();
    m_monitoredTable->setColumnCount(3);
    m_monitoredTable->setHorizontalHeaderLabels({tr("Display Name (Editable)"), tr("Systemd Unit"), tr("Current Status")});
    m_monitoredTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_monitoredTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_monitoredTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_monitoredTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_monitoredTable->setAlternatingRowColors(true);
    tab1Layout->addWidget(m_monitoredTable);

    auto *hintMonitored = new QLabel(tr("💡 Reorder items using Move Up/Down to change their order in your system tray menu."));
    hintMonitored->setStyleSheet("color: gray; font-size: 11px;");
    tab1Layout->addWidget(hintMonitored);

    m_tabWidget->addTab(tab1, tr("📌 Monitored in Tray (%1)").arg(m_config.services.size()));

    // ==================== Tab 2: All System Services ====================
    auto *tab2 = new QWidget();
    auto *tab2Layout = new QVBoxLayout(tab2);

    auto *searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel(tr("🔍 Filter:")));
    m_explorerSearch = new QLineEdit();
    m_explorerSearch->setPlaceholderText(tr("Search by service name or description (e.g. docker, nginx, mariadb)..."));
    m_explorerSearch->setClearButtonEnabled(true);
    searchLayout->addWidget(m_explorerSearch);

    m_lblExplorerCount = new QLabel();
    searchLayout->addWidget(m_lblExplorerCount);
    tab2Layout->addLayout(searchLayout);

    m_explorerTable = new QTableWidget();
    m_explorerTable->setColumnCount(4);
    m_explorerTable->setHorizontalHeaderLabels({tr("Monitor"), tr("Service Unit"), tr("Status"), tr("Description")});
    m_explorerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_explorerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_explorerTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_explorerTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_explorerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_explorerTable->setAlternatingRowColors(true);
    tab2Layout->addWidget(m_explorerTable);

    m_tabWidget->addTab(tab2, tr("🔍 All System Services (%1)").arg(m_allServices.size()));

    // ==================== Tab 3: General Options ====================
    auto *tab3 = new QWidget();
    auto *tab3Layout = new QVBoxLayout(tab3);
    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(14);

    m_spinInterval = new QSpinBox();
    m_spinInterval->setRange(1, 60);
    m_spinInterval->setValue(m_config.refreshIntervalSecs);
    m_spinInterval->setSuffix(tr(" seconds"));
    formLayout->addRow(tr("Status Refresh Interval:"), m_spinInterval);

    m_comboPresetIcons = new QComboBox();
    for (int i = 0; POPULAR_ICONS[i].id != nullptr; ++i) {
        QIcon icon = QIcon::fromTheme(POPULAR_ICONS[i].id);
        if (icon.isNull() && QString(POPULAR_ICONS[i].id) == "unitray") {
            QString homePix = QDir::homePath() + "/.local/share/pixmaps/unitray.png";
            if (QFile::exists(homePix)) icon = QIcon(homePix);
        }
        m_comboPresetIcons->addItem(icon, QString("%1 (%2)").arg(POPULAR_ICONS[i].label, POPULAR_ICONS[i].id), POPULAR_ICONS[i].id);
    }
    formLayout->addRow(tr("Tray Icon Preset:"), m_comboPresetIcons);

    auto *customIconLayout = new QHBoxLayout();
    m_lblIconPreview = new QLabel();
    m_lblIconPreview->setFixedSize(30, 30);
    customIconLayout->addWidget(m_lblIconPreview);

    m_editCustomIcon = new QLineEdit(m_config.trayIcon);
    customIconLayout->addWidget(m_editCustomIcon);

    m_btnBrowseIcons = new QPushButton(tr("🔍 Browse System Icons..."));
    customIconLayout->addWidget(m_btnBrowseIcons);
    formLayout->addRow(tr("Tray Icon Name:"), customIconLayout);

    m_btnOpenConfig = new QPushButton(tr("📂 Open config.toml in External Editor"));
    formLayout->addRow(tr("Configuration File:"), m_btnOpenConfig);

    tab3Layout->addLayout(formLayout);
    tab3Layout->addStretch();
    m_tabWidget->addTab(tab3, tr("⚙ General Options"));

    // Select initial icon preset
    for (int i = 0; i < m_comboPresetIcons->count(); ++i) {
        if (m_comboPresetIcons->itemData(i).toString() == m_config.trayIcon) {
            m_comboPresetIcons->setCurrentIndex(i);
            break;
        }
    }
    onCustomIconTextChanged(m_config.trayIcon);

    // ==================== Bottom Controls Bar ====================
    auto *bottomBar = new QHBoxLayout();
    m_lblStatus = new QLabel();
    bottomBar->addWidget(m_lblStatus);
    bottomBar->addStretch();

    m_btnCancel = new QPushButton(tr("Cancel"));
    m_btnSave = new QPushButton(tr("💾 Save & Apply"));
    m_btnSave->setDefault(true);
    bottomBar->addWidget(m_btnCancel);
    bottomBar->addWidget(m_btnSave);
    mainLayout->addLayout(bottomBar);

    // Connections
    connect(m_btnAddCustom, &QPushButton::clicked, this, &SettingsWindow::onAddCustomService);
    connect(m_btnMoveUp, &QPushButton::clicked, this, &SettingsWindow::onMoveUp);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &SettingsWindow::onMoveDown);
    connect(m_btnEditName, &QPushButton::clicked, this, &SettingsWindow::onEditName);
    connect(m_btnRemove, &QPushButton::clicked, this, &SettingsWindow::onRemoveSelected);

    connect(m_explorerSearch, &QLineEdit::textChanged, this, &SettingsWindow::onExplorerSearchChanged);
    connect(m_explorerTable, &QTableWidget::cellChanged, this, &SettingsWindow::onExplorerCellChanged);

    connect(m_comboPresetIcons, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsWindow::onPresetIconChanged);
    connect(m_editCustomIcon, &QLineEdit::textChanged, this, &SettingsWindow::onCustomIconTextChanged);
    connect(m_btnBrowseIcons, &QPushButton::clicked, this, &SettingsWindow::onBrowseSystemIcons);
    connect(m_btnOpenConfig, &QPushButton::clicked, this, &SettingsWindow::onOpenConfigFile);

    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnSave, &QPushButton::clicked, this, &SettingsWindow::onSaveAndApply);
}

void SettingsWindow::populateMonitoredTable() {
    m_monitoredTable->setRowCount(0);
    for (const auto &s : m_config.services) {
        int row = m_monitoredTable->rowCount();
        m_monitoredTable->insertRow(row);

        auto *nameItem = new QTableWidgetItem(s.name);
        m_monitoredTable->setItem(row, 0, nameItem);

        auto *unitItem = new QTableWidgetItem(s.unit);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        m_monitoredTable->setItem(row, 1, unitItem);

        bool isActive = m_allServices.contains(s.unit) ? m_allServices[s.unit].isActive : false;
        auto *statusItem = new QTableWidgetItem(isActive ? tr("🟢 Active") : tr("🔴 Inactive"));
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        m_monitoredTable->setItem(row, 2, statusItem);
    }
    m_tabWidget->setTabText(0, tr("📌 Monitored in Tray (%1)").arg(m_config.services.size()));
}

void SettingsWindow::populateExplorerTable() {
    m_updatingExplorer = true;
    m_explorerTable->setRowCount(0);

    QSet<QString> monitoredSet;
    for (const auto &s : m_config.services) {
        monitoredSet.insert(s.unit);
    }

    // Sort services: monitored ones first, then alphabetical
    QStringList units = m_allServices.keys();
    std::sort(units.begin(), units.end(), [&monitoredSet](const QString &a, const QString &b) {
        bool aMon = monitoredSet.contains(a);
        bool bMon = monitoredSet.contains(b);
        if (aMon != bMon) return aMon;
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });

    for (const QString &unit : units) {
        int row = m_explorerTable->rowCount();
        m_explorerTable->insertRow(row);

        bool isMonitored = monitoredSet.contains(unit);
        auto *checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        checkItem->setCheckState(isMonitored ? Qt::Checked : Qt::Unchecked);
        checkItem->setTextAlignment(Qt::AlignCenter);
        m_explorerTable->setItem(row, 0, checkItem);

        auto *unitItem = new QTableWidgetItem(unit);
        unitItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_explorerTable->setItem(row, 1, unitItem);

        const auto &info = m_allServices[unit];
        auto *statusItem = new QTableWidgetItem(info.isActive ? tr("🟢 Active") : tr("🔴 Inactive"));
        statusItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_explorerTable->setItem(row, 2, statusItem);

        auto *descItem = new QTableWidgetItem(info.description);
        descItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_explorerTable->setItem(row, 3, descItem);
    }

    m_updatingExplorer = false;
    onExplorerSearchChanged(m_explorerSearch->text());
}

void SettingsWindow::updateExplorerCheckboxes() {
    m_updatingExplorer = true;
    QSet<QString> monitoredSet;
    for (const auto &s : m_config.services) {
        monitoredSet.insert(s.unit);
    }

    for (int row = 0; row < m_explorerTable->rowCount(); ++row) {
        QString unit = m_explorerTable->item(row, 1)->text();
        m_explorerTable->item(row, 0)->setCheckState(monitoredSet.contains(unit) ? Qt::Checked : Qt::Unchecked);
    }
    m_updatingExplorer = false;
}

void SettingsWindow::onExplorerSearchChanged(const QString &text) {
    QString query = text.trimmed().toLower();
    int visibleCount = 0;

    for (int row = 0; row < m_explorerTable->rowCount(); ++row) {
        QString unit = m_explorerTable->item(row, 1)->text().toLower();
        QString desc = m_explorerTable->item(row, 3)->text().toLower();
        bool match = query.isEmpty() || unit.contains(query) || desc.contains(query);
        m_explorerTable->setRowHidden(row, !match);
        if (match) visibleCount++;
    }

    m_lblExplorerCount->setText(tr("Showing %1 of %2").arg(visibleCount).arg(m_explorerTable->rowCount()));
}

void SettingsWindow::onExplorerCellChanged(int row, int column) {
    if (m_updatingExplorer || column != 0) return;

    QString unit = m_explorerTable->item(row, 1)->text();
    bool isChecked = (m_explorerTable->item(row, 0)->checkState() == Qt::Checked);

    int foundIdx = -1;
    for (int i = 0; i < m_config.services.size(); ++i) {
        if (m_config.services[i].unit == unit) {
            foundIdx = i;
            break;
        }
    }

    if (isChecked && foundIdx == -1) {
        m_config.services.append(MonitoredService{formatDisplayName(unit), unit});
        populateMonitoredTable();
    } else if (!isChecked && foundIdx != -1) {
        m_config.services.removeAt(foundIdx);
        populateMonitoredTable();
    }
}

void SettingsWindow::onAddCustomService() {
    AddCustomServiceDialog dlg(m_allServices.keys(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString unit = dlg.unitName();
        QString name = dlg.displayName();

        for (const auto &s : m_config.services) {
            if (s.unit == unit) {
                QMessageBox::information(this, tr("Info"), tr("Service %1 is already in your monitored list.").arg(unit));
                return;
            }
        }

        m_config.services.append(MonitoredService{name, unit});
        populateMonitoredTable();
        updateExplorerCheckboxes();
    }
}

void SettingsWindow::onMoveUp() {
    int row = m_monitoredTable->currentRow();
    if (row > 0 && row < m_config.services.size()) {
        m_config.services.swapItemsAt(row, row - 1);
        populateMonitoredTable();
        m_monitoredTable->selectRow(row - 1);
    }
}

void SettingsWindow::onMoveDown() {
    int row = m_monitoredTable->currentRow();
    if (row >= 0 && row + 1 < m_config.services.size()) {
        m_config.services.swapItemsAt(row, row + 1);
        populateMonitoredTable();
        m_monitoredTable->selectRow(row + 1);
    }
}

void SettingsWindow::onRemoveSelected() {
    int row = m_monitoredTable->currentRow();
    if (row >= 0 && row < m_config.services.size()) {
        m_config.services.removeAt(row);
        populateMonitoredTable();
        updateExplorerCheckboxes();
    }
}

void SettingsWindow::onEditName() {
    int row = m_monitoredTable->currentRow();
    if (row >= 0 && row < m_config.services.size()) {
        QString currentName = m_config.services[row].name;
        bool ok = false;
        QString newName = QInputDialog::getText(this, tr("Edit Display Name"), tr("Display Name:"), QLineEdit::Normal, currentName, &ok);
        if (ok && !newName.trimmed().isEmpty()) {
            m_config.services[row].name = newName.trimmed();
            populateMonitoredTable();
            m_monitoredTable->selectRow(row);
        }
    }
}

void SettingsWindow::onPresetIconChanged(int index) {
    if (index >= 0) {
        QString iconId = m_comboPresetIcons->itemData(index).toString();
        m_editCustomIcon->setText(iconId);
    }
}

void SettingsWindow::onCustomIconTextChanged(const QString &text) {
    m_config.trayIcon = text.trimmed();
    QIcon icon = QIcon::fromTheme(m_config.trayIcon);
    if (icon.isNull() && m_config.trayIcon == "unitray") {
        QString homePix = QDir::homePath() + "/.local/share/pixmaps/unitray.png";
        if (QFile::exists(homePix)) icon = QIcon(homePix);
    }
    if (!icon.isNull()) {
        m_lblIconPreview->setPixmap(icon.pixmap(28, 28));
    } else {
        m_lblIconPreview->clear();
    }
}

void SettingsWindow::onBrowseSystemIcons() {
    IconPickerDialog dlg(m_discoveredIcons, m_editCustomIcon->text().trimmed(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString selected = dlg.selectedIcon();
        if (!selected.isEmpty()) {
            m_editCustomIcon->setText(selected);
            for (int i = 0; i < m_comboPresetIcons->count(); ++i) {
                if (m_comboPresetIcons->itemData(i).toString() == selected) {
                    m_comboPresetIcons->setCurrentIndex(i);
                    break;
                }
            }
        }
    }
}

void SettingsWindow::onOpenConfigFile() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(configFilePath()));
}

void SettingsWindow::onSaveAndApply() {
    // Sync any edits directly from table cells
    for (int row = 0; row < m_monitoredTable->rowCount(); ++row) {
        if (row < m_config.services.size()) {
            m_config.services[row].name = m_monitoredTable->item(row, 0)->text().trimmed();
        }
    }

    m_config.refreshIntervalSecs = m_spinInterval->value();
    m_config.trayIcon = m_editCustomIcon->text().trimmed();
    if (m_config.trayIcon.isEmpty()) m_config.trayIcon = "unitray";

    if (saveConfig()) {
        QMessageBox::information(
            this,
            tr("Success"),
            tr("Configuration saved successfully with %1 monitored services!\n\n"
               "Unitray has automatically applied the changes to your system tray.")
               .arg(m_config.services.size())
        );
        accept();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to write configuration file."));
    }
}
