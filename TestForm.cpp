#include "TestForm.h"
#include "ui_TestForm.h"

TestForm::TestForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::TestForm)
{
	ui->setupUi(this);
}

TestForm::~TestForm()
{
	delete ui;
}
