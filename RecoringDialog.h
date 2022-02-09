#ifndef RECORINGDIALOG_H
#define RECORINGDIALOG_H

#include <QDialog>

namespace Ui {
class RecoringDialog;
}

class RecoringDialog : public QDialog {
	Q_OBJECT
public:
	explicit RecoringDialog(QWidget *parent = nullptr);
	~RecoringDialog();
	QTime maximumLength() const;
private:
	Ui::RecoringDialog *ui;
};

#endif // RECORINGDIALOG_H
