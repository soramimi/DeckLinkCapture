#ifndef RECORINGDIALOG_H
#define RECORINGDIALOG_H

#include <QDialog>

namespace Ui {
class RecoringDialog;
}

class RecoringDialog : public QDialog {
	Q_OBJECT
private:
	Ui::RecoringDialog *ui;
public:
	explicit RecoringDialog(QWidget *parent = nullptr);
	~RecoringDialog();
	QTime maximumLength() const;
	QString path() const;
private slots:
	void on_pushButton_browse_clicked();
public slots:
	void done(int v);
};

#endif // RECORINGDIALOG_H
