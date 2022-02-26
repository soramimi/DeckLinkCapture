#ifndef ACTIONHANDLER_H
#define ACTIONHANDLER_H

#include <QObject>
#include <functional>

class MainWindow;
class QAction;

class ActionHandler : public QObject {
	Q_OBJECT
private:
	QString name_;
	std::function<void (QString const &name)> callback_fn;
public:
	explicit ActionHandler(QObject *parent, QAction *action, QString const &name, const std::function<void (const QString &)> fn);
public slots:
	void triggered(bool checked);
};

#endif // ACTIONHANDLER_H
