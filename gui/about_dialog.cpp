#include "about_dialog.h"
#include "app_icon.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>

namespace {
constexpr const char *GITHUB_URL = "https://github.com/Khuirul-Huda/unitray";
constexpr const char *VERSION_STRING = "v0.1.0";
} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

void AboutDialog::setupUi() {
    setWindowTitle(tr("About Unitray"));
    setWindowIcon(resolveAppIcon());
    resize(460, 380);
    setMinimumSize(420, 340);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 20);

    // Header section with icon and app title
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);

    auto *iconLabel = new QLabel(this);
    const QIcon appIcon = resolveAppIcon();
    iconLabel->setPixmap(appIcon.pixmap(64, 64));
    iconLabel->setFixedSize(64, 64);
    headerLayout->addWidget(iconLabel);

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    auto *nameLabel = new QLabel(this);
    nameLabel->setText(QStringLiteral("<h2 style='margin:0;'>Unitray <span style='font-size:13px; color:#7f8c8d; font-weight:normal;'>%1</span></h2>").arg(VERSION_STRING));
    nameLabel->setTextFormat(Qt::RichText);
    titleLayout->addWidget(nameLabel);

    auto *descLabel = new QLabel(tr("Lightweight Linux Systemd Service Controller"), this);
    descLabel->setStyleSheet("color: #7f8c8d; font-size: 12px;");
    descLabel->setWordWrap(true);
    titleLayout->addWidget(descLabel);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Separator line
    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Details content
    auto *infoLabel = new QLabel(this);
    infoLabel->setTextFormat(Qt::RichText);
    infoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setWordWrap(true);
    infoLabel->setText(
        tr("<p style='line-height: 1.5; margin: 0;'>"
           "<b>Author:</b> Khuirul Huda (<code>@Khuirul-Huda</code>)<br>"
           "<b>Contact:</b> <a href='mailto:hello@khuirulhuda.my.id'>hello@khuirulhuda.my.id</a><br>"
           "<b>GitHub:</b> <a href='https://github.com/Khuirul-Huda/unitray'>github.com/Khuirul-Huda/unitray</a><br>"
           "<b>License:</b> MIT License<br>"
           "<b>Architecture:</b> Pure Rust Tray Daemon &amp; Native Qt 6 GUI"
           "</p>")
    );
    mainLayout->addWidget(infoLabel);

    mainLayout->addStretch();

    // Bottom action buttons
    auto *bottomLayout = new QHBoxLayout();
    auto *btnGitHub = new QPushButton(tr("🌐 GitHub Repository"), this);
    auto *btnClose = new QPushButton(tr("Close"), this);
    btnClose->setDefault(true);

    bottomLayout->addWidget(btnGitHub);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnClose);
    mainLayout->addLayout(bottomLayout);

    connect(btnGitHub, &QPushButton::clicked, this, &AboutDialog::onOpenGitHub);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void AboutDialog::onOpenGitHub() {
    QDesktopServices::openUrl(QUrl(GITHUB_URL));
}
