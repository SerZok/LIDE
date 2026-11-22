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

private slots:
	void onBrowseSBCLPath();
	void onBrowseProjectPath();
	void fetchAiModels();
	void fetchAiEmbeddingModels();
	void addContextFiles();
	void removeContextFiles();

protected:
	void changeEvent(QEvent* event) override;

private:
	Ui::SettingsDialog ui;

    Settings* m_settings;
    bool m_loading = false;

    void loadToUi();
    void saveFromUi();

	void updateSbclArgs();
	void updateSbclLoad();
	void fetchModels(bool embedding);
};
