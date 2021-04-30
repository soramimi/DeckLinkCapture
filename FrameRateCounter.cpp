#include "FrameRateCounter.h"

#include <QDebug>

FrameRateCounter::FrameRateCounter()
{

}

void FrameRateCounter::stop()
{
	this->requestInterruption();
	this->waiter_.wakeAll();
	this->wait();
}

void FrameRateCounter::run()
{
	while (1) {
		{
			QMutexLocker lock(&mutex_);
			waiter_.wait(&mutex_, 1000);
		}
		if (isInterruptionRequested()) break;
		fps_ = counter_;
		counter_ = 0;
		qDebug() << fps_;
	}
}

void FrameRateCounter::increment()
{
	counter_++;
}

int FrameRateCounter::fps()
{
	return fps_;
}
