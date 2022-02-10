#ifndef TESTFORM_H
#define TESTFORM_H

#include <QWidget>

namespace Ui {
class TestForm;
}

class TestForm : public QWidget
{
	Q_OBJECT

public:
	explicit TestForm(QWidget *parent = nullptr);
	~TestForm();

private:
	Ui::TestForm *ui;
};

#endif // TESTFORM_H
