#include "Deinterlace.h"
#include <memory>

static inline int gray(int r, int g, int b)
{
	return (r * 306 + g * 601 + b * 117) / 1024;
}

void DeinterlaceThread::process(const DeinterlaceThread::Task &task)
{
	const int span = 7;
	const int margin = (span / 2 + 1) * 2;
	using int16x2 = int16_t[2];
	uint8_t *buf0 = (uint8_t *)alloca(task.length + margin * 2);
	uint8_t *buf1 = (uint8_t *)alloca(task.length + margin * 2);
	int16x2 *vec = (int16x2 *)alloca(task.length * sizeof(int16x2));
	for (int x = 0; x < task.length; x++) {
		int r0 = task.src0[x * 3 + 0];
		int g0 = task.src0[x * 3 + 1];
		int b0 = task.src0[x * 3 + 2];
		int r1 = task.src1[x * 3 + 0];
		int g1 = task.src1[x * 3 + 1];
		int b1 = task.src1[x * 3 + 2];
		buf0[x + margin] = gray(r0, g0, b0);
		buf1[x + margin] = gray(r1, g1, b1);
	}
	memset(buf0, buf0[margin], margin);
	memset(buf1, buf1[margin], margin);
	memset(buf0 + margin + task.length, buf0[task.length + margin - 1], margin);
	memset(buf1 + margin + task.length, buf1[task.length + margin - 1], margin);
	for (int x = 0; x < task.length; x++) {
		vec[x][0] = (int16_t)0x8000;
	}
	for (int x = 0; x < task.length; x++) {
		int index = 0;
		int distance = 2000000;
		for (int i = 0; i < span; i++) {
			int j = i - span / 2;
			int x0 = x + margin;
			int x1 = x + margin + j;
			int a = (int)buf0[x0 - 1] - (int)buf1[x1 - 1];
			int b = (int)buf0[x0    ] - (int)buf1[x1    ];
			int c = (int)buf0[x0 + 1] - (int)buf1[x1 + 1];
			int d = a * a + b * b + c * c;
			if (distance > d) {
				distance = d;
				index = j;
			}
		}
		vec[x + index / 2][0] = (int16_t)index;
	}
	{
		int t = 0;
		int u = 0;
		for (int x = 0; x < task.length; x++) {
			if ((uint16_t)vec[x][0] != 0x8000) {
				int v = vec[x][0];
				t = x - v / 2;
				u = x + (v + 1) / 2;
				t = t < 0 ? 0 : (t > task.length ? task.length : t);
				u = u < 0 ? 0 : (u > task.length ? task.length : u);
			}
			vec[x][0] = t;
			vec[x][1] = u;
		}
	}
	for (int x = 0; x < task.length; x++) {
		int t = vec[x][0];
		int u = vec[x][1];
		uint8_t r = (task.src0[t * 3 + 0] + task.src1[u * 3 + 0]) / 2;
		uint8_t g = (task.src0[t * 3 + 1] + task.src1[u * 3 + 1]) / 2;
		uint8_t b = (task.src0[t * 3 + 2] + task.src1[u * 3 + 2]) / 2;
		task.dst[x * 3 + 0] = r;
		task.dst[x * 3 + 1] = g;
		task.dst[x * 3 + 2] = b;
	}
}

void DeinterlaceThread::run()
{
	while (1) {
		Task t;
		{
			QMutexLocker lock(&that->mutex_);
			if (that->index_ < that->tasklist_.size()) {
				t = that->tasklist_[that->index_];
				that->index_++;
			} else {
				break;
			}
		}
		DeinterlaceThread::process(t);
	}
}

DeinterlaceThread::DeinterlaceThread(Deinterlace *di)
	: that(di)
{
}

std::pair<QImage, QImage> Deinterlace::filter(QImage image)
{
	QImage newimage0;
	QImage newimage1;
	image = image.convertToFormat(QImage::Format_RGB888);
	int w = image.width();
	int h = image.height();
	if (w > 0 && h > 2) {
		newimage0 = QImage(w, h, QImage::Format_RGB888);
		newimage1 = QImage(w, h, QImage::Format_RGB888);
		{
			for (int y = 0; y + 2 < h; y += 2) {
				tasklist_.emplace_back(w, image.scanLine(y), image.scanLine(y + 2), newimage0.scanLine(y + 1));
			}
			for (int y = 0; y + 3 < h; y += 2) {
				tasklist_.emplace_back(w, image.scanLine(y + 1), image.scanLine(y + 3), newimage1.scanLine(y + 2));
			}
			for (int i = 0; i < THREAD_COUNT; i++) {
				std::shared_ptr<DeinterlaceThread> th = std::make_shared<DeinterlaceThread>(this);
				th->start();
				threads_.push_back(th);
			}
			{
				for (int y = 0; y < h; y += 2) {
					memcpy(newimage0.scanLine(y), image.scanLine(y), w * 3);
				}
				for (int y = 0; y + 1 < h; y += 2) {
					memcpy(newimage1.scanLine(y + 1), image.scanLine(y + 1), w * 3);
				}
				memcpy(newimage0.scanLine(h - 1), newimage0.scanLine(h - 2), w * 3);
				memcpy(newimage1.scanLine(0), newimage1.scanLine(1), w * 3);
			}
			for (auto th : threads_) {
				th->wait();
				th.reset();
			}
		}
	}
	return std::make_pair<QImage, QImage>(std::move(newimage0), std::move(newimage1));
}

