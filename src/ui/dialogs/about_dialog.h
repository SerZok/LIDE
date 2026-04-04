#pragma once

#include <QDialog>
#include "ui/ui_about_dialog.h"

class AboutDialog : public QDialog
{
	Q_OBJECT

public:
	AboutDialog(QWidget *parent = nullptr);
	~AboutDialog();

protected:
	void changeEvent(QEvent* event) override;

private:
	Ui::AboutDialog ui;
};
