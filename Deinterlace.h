#ifndef DEINTERLACE_H
#define DEINTERLACE_H

#include <QMutex>
#include <stdint.h>
#include <vector>
#include <memory>
#include <QImage>
#include <QThread>

class Deinterlace {
	friend class DeinterlaceThread;
private:
	const int THREAD_COUNT = 8;

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
	QMutex mutex_;
	std::vector<Task> tasklist_;
	int index_ = 0;
	std::vector<std::shared_ptr<DeinterlaceThread>> threads_;
public:
	std::pair<QImage, QImage> filter(QImage image);
};

class DeinterlaceThread : public QThread {
	friend class Deinterlace;
private:
	using Task = Deinterlace::Task;
	Deinterlace *that = nullptr;
	static void process(Task const &task);
	void run();
public:
	DeinterlaceThread(Deinterlace *di);
};


#endif // DEINTERLACE_H
