
// Deinterlace filter
// Copyright (C) 2019 S.Fuchita (@soramimi_jp)
// MIT License

#ifndef DEINTERLACE_H
#define DEINTERLACE_H

#include <QMutex>
#include <stdint.h>
#include <vector>
#include <memory>
#include <QImage>
#include <QThread>

class DeinterlaceThread;

class Deinterlace {
	friend class DeinterlaceThread;
private:
	const int THREAD_COUNT = 4;
	struct Task {
		int job = 0;
		int y = 0;
		int h = 0;
		int w = 0;
		uint8_t *dst = nullptr;
		uint8_t const *src0 = nullptr;
		uint8_t const *src1 = nullptr;
	};
	struct Private;
	Private *m;
	void clear();
public:
	Deinterlace();
	~Deinterlace();
	void start();
	void stop();
	std::pair<QImage, QImage> filter(QImage image);
};

class DeinterlaceThread : public QThread {
	friend class Deinterlace;
private:
	using Task = Deinterlace::Task;
	Deinterlace *that = nullptr;
	int number_ = 0;
	void process(Task &task);
	void run();
public:
	DeinterlaceThread(Deinterlace *di, int number);
};

#endif // DEINTERLACE_H
