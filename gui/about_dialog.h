#pragma once

#include <QDialog>

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog() override = default;

private slots:
    void onOpenGitHub();

private:
    void setupUi();
};
