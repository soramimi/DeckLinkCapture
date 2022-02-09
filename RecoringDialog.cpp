#include "RecoringDialog.h"
#include "ui_RecoringDialog.h"

RecoringDialog::RecoringDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::RecoringDialog)
{
	ui->setupUi(this);

	ui->timeEdit->setTime(QTime(3, 0, 0));
}

RecoringDialog::~RecoringDialog()
{
	delete ui;
}

QTime RecoringDialog::maximumLength() const
{
	return ui->timeEdit->time();
}
