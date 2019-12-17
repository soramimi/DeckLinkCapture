
// Deinterlace filter
// Copyright (C) 2019 S.Fuchita (@soramimi_jp)
// MIT License

#include "Deinterlace.h"
#include <QDebug>
#include <QElapsedTimer>
#include <memory>

#ifdef USE_OPENCV
#include <opencv2/imgproc/imgproc.hpp>
#endif

struct Deinterlace::Private {
	QMutex mutex;
	volatile unsigned int busy = 0;
	unsigned int index = 0;
	std::vector<int8_t> vec;
	std::vector<uint8_t> flags;
	std::vector<Task> tasklist;
	std::vector<std::shared_ptr<DeinterlaceThread>> threads;
};

void DeinterlaceThread::process(DeinterlaceThread::Task &task)
{
	switch (task.job) {
	case 0:
		{
			const int w = task.w;
			const int h = task.h;
			const int y = task.y;
			uint8_t const *src = task.src0;
			uint8_t *flags = &that->m->flags[w * y];
			for (int x = 0; x < w; x++) {
				uint8_t const *s = src + x;
				uint8_t a = s[w * 0];
				uint8_t b = s[w * 1];
				uint8_t c = s[w * 2];
				uint8_t d = s[w * 3];
				if ((b < c && a <= b && c <= d) || (c < b && b <= a && d <= c)) {
					flags[w * 0 + x] = 0;
					flags[w * 1 + x] = 0;
					flags[w * 2 + x] = 0;
					flags[w * 3 + x] = 0;
				} else if (y + 4 < h) {
					uint8_t e = s[w * 4];
					uint8_t t = (b + c * 2 + d) / 4;
					if ((b < c && d < c && a < t && e < t) || (b > c && d > c && a > t && e > t)) {
						flags[w * 1 + x] = 0;
						flags[w * 2 + x] = 0;
						flags[w * 3 + x] = 0;
					}
				}
			}
		}
		break;
	case 1:
		{
			const int w = task.w;
			const int span = 7;
			const int margin = span / 2;
			uint8_t *buf0 = (uint8_t *)alloca(w + margin * 2);
			uint8_t *buf1 = (uint8_t *)alloca(w + margin * 2);
			int8_t *v = &that->m->vec[task.w * task.y];
			memcpy(buf0 + margin, task.src0, w);
			memcpy(buf1 + margin, task.src1, w);
			memset(buf0, buf0[margin], margin);
			memset(buf1, buf1[margin], margin);
			memset(buf0 + margin + w, buf0[w + margin - 1], margin);
			memset(buf1 + margin + w, buf1[w + margin - 1], margin);
			for (int x = 0; x < w; x++) {
				int offset = 0;
				int distance = 2000000;
				for (int i = 0; i < span; i++) {
					int o = i - span / 2;
					int x0 = x + margin;
					int x1 = x + margin + o;
					int a = (int)buf0[x0 - 1] - (int)buf1[x1 - 1];
					int b = (int)buf0[x0    ] - (int)buf1[x1    ];
					int c = (int)buf0[x0 + 1] - (int)buf1[x1 + 1];
					int d = a * a + b * b + c * c;
					if (distance > d) {
						distance = d;
						offset = o;
					}
				}
				v[x + offset / 2] = (int8_t)offset;
			}
		}
		break;
	case 2:
		{
			const int w = task.w;
			int8_t *v = &that->m->vec[w * task.y];
			uint8_t const *f = &that->m->flags[w * task.y];
			int t = 0;
			int u = 0;
			for (int x = 0; x < w; x++) {
				if (f[x] & 1) {
					if ((uint8_t)v[x] != (uint8_t)0x80) {
						int offset = v[x];
						t = x - offset / 2;
						u = x + (offset + 1) / 2;
						t = t < 0 ? 0 : (t > w ? w : t);
						u = u < 0 ? 0 : (u > w ? w : u);
					}
					uint8_t const *s0 = task.src0 + t * 3;
					uint8_t const *s1 = task.src1 + u * 3;
					uint8_t *d = task.dst + x * 3;
					d[0] = (s0[0] + s1[0]) / 2;
					d[1] = (s0[1] + s1[1]) / 2;
					d[2] = (s0[2] + s1[2]) / 2;
				} else {
					t = u = x;
				}
			}
		}
		break;
	}
}

void DeinterlaceThread::run()
{
	while (!isInterruptionRequested()) {
		Task t;
		{
			QMutexLocker lock(&that->m->mutex);
			if (that->m->index < that->m->tasklist.size()) {
				t = that->m->tasklist[that->m->index];
				that->m->index++;
			} else {
				that->m->busy &= ~(1 << number_);
			}
		}
		if (t.w > 0) {
			process(t);
		} else {
			QThread::yieldCurrentThread();
		}
	}
	QMutexLocker lock(&that->m->mutex);
	that->m->busy &= ~(1 << number_);
}

DeinterlaceThread::DeinterlaceThread(Deinterlace *di, int number)
	: that(di)
	, number_(number)
{
}

Deinterlace::Deinterlace()
	: m(new Private)
{
}

Deinterlace::~Deinterlace()
{
	stop();
	delete m;
}

void Deinterlace::start()
{
	if (m->threads.empty()) {
		for (int i = 0; i < THREAD_COUNT; i++) {
			std::shared_ptr<DeinterlaceThread> th = std::make_shared<DeinterlaceThread>(this, i);
			m->threads.push_back(th);
			th->start();
		}
	}
}

void Deinterlace::clear()
{
	m->tasklist.clear();
	m->index = 0;
}

void Deinterlace::stop()
{
	for (auto th : m->threads) {
		th->requestInterruption();
	}
	for (auto th : m->threads) {
		if (!th->wait(500)) {
			th->terminate();
		}
	}
	m->threads.clear();
	clear();
}

std::pair<QImage, QImage> Deinterlace::filter(QImage image)
{
	QImage newimage0;
	QImage newimage1;
	image = image.convertToFormat(QImage::Format_RGB888);
	int w = image.width();
	int h = image.height();
	if (w > 0 && h > 2) {

#ifdef USE_OPENCV
		cv::Mat mat_rgb(h, w, CV_8UC3, (void *)image.bits());
		cv::Mat mat_gray;
		cv::cvtColor(mat_rgb, mat_gray, cv::COLOR_RGB2GRAY);
		auto GrayscaleLine = [&](int y){
			return mat_gray.ptr(y);
		};
#else
		QImage grayed = image.convertToFormat(QImage::Format_Grayscale8);
		auto GrayscaleLine = [&](int y){
			return grayed.scanLine(y);
		};
#endif
		{
			const int wh = w * h;
			if ((int)m->vec.size() != wh) m->vec.resize(wh);
			if ((int)m->flags.size() != wh) m->flags.resize(wh);
			memset(&m->vec[0], 0x80, wh);
			memset(&m->flags[0], 1, wh);

			{
				QMutexLocker lock(&m->mutex);
				clear();

				// stage a
				for (int y = 0; y + 3 < h; y++) {
					DeinterlaceThread::Task t;
					t.job = 0;
					t.y = y;
					t.h = h;
					t.w = w;
					t.src0 = GrayscaleLine(y);
					m->tasklist.push_back(t);
				}

				// stage b-1
				for (int y = 0; y + 2 < h; y += 2) {
					DeinterlaceThread::Task t;
					t.job = 1;
					t.y = y + 1;
					t.h = h;
					t.w = w;
					t.src0 = GrayscaleLine(y);
					t.src1 = GrayscaleLine(y + 2);
					m->tasklist.push_back(t);
				}

				// stage b-2
				for (int y = 1; y + 2 < h; y += 2) {
					DeinterlaceThread::Task t;
					t.job = 1;
					t.y = y + 1;
					t.h = h;
					t.w = w;
					t.src0 = GrayscaleLine(y);
					t.src1 = GrayscaleLine(y + 2);
					m->tasklist.push_back(t);
				}

				m->busy = (1 << m->threads.size()) - 1;
			}

			newimage0 = image.copy();
			newimage1 = image.copy();

			while (m->busy != 0) {
				QThread::yieldCurrentThread();
			}

			{
				QMutexLocker lock(&m->mutex);
				clear();

				// stage c-1
				for (int y = 0; y + 2 < h; y += 2) {
					DeinterlaceThread::Task t;
					t.job = 2;
					t.y = y + 1;
					t.h = h;
					t.w = w;
					t.src0 = newimage0.scanLine(y);
					t.src1 = newimage0.scanLine(y + 2);
					t.dst = newimage0.scanLine(y + 1);
					m->tasklist.push_back(t);
				}

				// stage c-2
				for (int y = 1; y + 2 < h; y += 2) {
					DeinterlaceThread::Task t;
					t.job = 2;
					t.y = y + 1;
					t.h = h;
					t.w = w;
					t.src0 = newimage1.scanLine(y);
					t.src1 = newimage1.scanLine(y + 2);
					t.dst = newimage1.scanLine(y + 1);
					m->tasklist.push_back(t);
				}

				m->busy = (1 << m->threads.size()) - 1;
			}

			while (m->busy != 0) {
				QThread::yieldCurrentThread();
			}
		}
	}
	return std::make_pair<QImage, QImage>(std::move(newimage0), std::move(newimage1));
}

