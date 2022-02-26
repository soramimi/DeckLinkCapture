#include "ActionHandler.h"
#include "MainWindow.h"
#include <QAction>

ActionHandler::ActionHandler(QObject *parent, QAction *action, const QString &name, const std::function<void (const QString &)> fn)
	: QObject(parent)
	, name_(name)
	, callback_fn(fn)
{
	connect(action, &QAction::triggered, this, &ActionHandler::triggered);
}

void ActionHandler::triggered(bool checked)
{
	callback_fn(name_);
}
