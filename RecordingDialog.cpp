#include "RecordingDialog.h"
#include "ui_RecoringDialog.h"
#include "MySettings.h"
#include <QFileDialog>

RecordingDialog::RecordingDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::RecoringDialog)
{
	ui->setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->timeEdit->setTime(QTime(3, 0, 0));

	{
		MySettings s;
		s.beginGroup("Global");
		{
			QString path = s.value("SaveVideoPath").toString();
			ui->lineEdit_path->setText(path);
		}
		{
			QString t = s.value("MaximumLength").toString();
			int h, m, s;
			h = m = s = 0;
			if (sscanf(t.toStdString().c_str(), "%d:%d:%d", &h, &m, &s) != 3) {
				h = 3;
				m = s = 0;
			}
			ui->timeEdit->setTime({h, m, s});
		}
		s.endGroup();
	}
}

RecordingDialog::~RecordingDialog()
{
	delete ui;
}

QTime RecordingDialog::maximumLength() const
{
	return ui->timeEdit->time();
}

QString RecordingDialog::path() const
{
	return ui->lineEdit_path->text();
}

void RecordingDialog::on_pushButton_browse_clicked()
{
	QString path = ui->lineEdit_path->text();
	path = QFileDialog::getSaveFileName(this, tr("Save as"), path);
	if (!path.isEmpty()) {
		ui->lineEdit_path->setText(path);
	}
}

void RecordingDialog::done(int v)
{
	if (v == QDialog::Accepted) {
		QTime t = ui->timeEdit->time();
		MySettings s;
		s.beginGroup("Global");
		s.setValue("SaveVideoPath", path());
		s.setValue("MaximumLength", QString::asprintf("%d:%02d:%02d", t.hour(), t.minute(), t.second()));
		s.endGroup();
	}
	QDialog::done(v);
}


