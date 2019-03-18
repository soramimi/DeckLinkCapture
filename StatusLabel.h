#ifndef STATUSLABEL_H
#define STATUSLABEL_H

#include <QLabel>

class StatusLabel : public QLabel {
public:
	StatusLabel(QWidget *parent = 0);
	QSize sizeHint() const
	{
		return QSize(0, QLabel::sizeHint().height());
	}
};

#endif // STATUSLABEL_H
