#pragma once

#include "settings.h"
#include "ui/ui_settings_dialog.h"

#include <QDialog>
#include <QTabWidget>

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	SettingsDialog(QWidget*parent = nullptr);
	~SettingsDialog();

private:
	Ui::SettingsDialog ui;

    Settings* m_settings;
    bool m_loading = false;

    void setupUi();
    void loadToUi();
    void saveFromUi();
};
