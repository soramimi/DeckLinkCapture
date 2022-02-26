#ifndef RECORDINGDIALOG_H
#define RECORDINGDIALOG_H

#include <QDialog>

namespace Ui {
class RecoringDialog;
}

class RecordingDialog : public QDialog {
	Q_OBJECT
private:
	Ui::RecoringDialog *ui;
public:
	explicit RecordingDialog(QWidget *parent = nullptr);
	~RecordingDialog();
	QTime maximumLength() const;
	QString path() const;
private slots:
	void on_pushButton_browse_clicked();
public slots:
	void done(int v);
};

#endif // RECORDINGDIALOG_H
