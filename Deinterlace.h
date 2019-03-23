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
		int length = 0;
		uint8_t const *src0 = nullptr;
		uint8_t const *src1 = nullptr;
		uint8_t *dst = nullptr;
		Task() = default;
		Task(int length, uint8_t const *src0, uint8_t const *src1, uint8_t *dst)
			: length(length)
			, src0(src0)
			, src1(src1)
			, dst(dst)
		{
		}
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
	static void process(Task const &task);
	void run();
public:
	DeinterlaceThread(Deinterlace *di, int number);
};


#endif // DEINTERLACE_H
