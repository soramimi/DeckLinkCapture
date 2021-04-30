#ifndef FRAMERATECOUNTER_H
#define FRAMERATECOUNTER_H

#include <QMutex>
#include <QThread>
#include <QWaitCondition>



class FrameRateCounter : public QThread {
private:
	int fps_ = 0;
	int counter_ = 0;
	QMutex mutex_;
	QWaitCondition waiter_;
protected:
	void run();
public:
	FrameRateCounter();
	void stop();
	void increment();
	int fps();
};

#endif // FRAMERATECOUNTER_H
