#include "about_dialog.h"
#include "version.h"

AboutDialog::AboutDialog(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	setFixedSize(size());

	QString versionText = tr("Версия: %1<br>Сборка: %2")
		.arg(APP_VERSION)
		.arg(BUILD_DATE);
	ui.versionLabel->setText(versionText);
}

AboutDialog::~AboutDialog()
{
}
