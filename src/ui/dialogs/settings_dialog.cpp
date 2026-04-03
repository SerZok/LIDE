#include "settings_dialog.h"

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent),
	m_settings(Settings::instance())
{
	ui.setupUi(this);

	//connect(ui->buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton* btn) {
	//	if (ui->buttonBox->standardButton(btn) == QDialogButtonBox::Apply) onApply();
	//	else if (ui->buttonBox->standardButton(btn) == QDialogButtonBox::Reset) onReset();
	//	});
}

SettingsDialog::~SettingsDialog()
{

}
