
// Deinterlace filter
// Copyright (C) 2019 S.Fuchita (@soramimi_jp)
// MIT License

#include "Deinterlace.h"
#include <QDebug>
#include <QElapsedTimer>
#include <memory>
#include <omp.h>

#ifdef USE_OPENCV
#include <opencv2/imgproc/imgproc.hpp>
#endif

struct Task {
	int y = 0;
	int h = 0;
	int w = 0;
	uint8_t *dst = nullptr;
	uint8_t const *src0 = nullptr;
	uint8_t const *src1 = nullptr;
};

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
			std::vector<int8_t> buf;
			std::vector<uint8_t> flags;

			const int wh = w * h;
			if ((int)buf.size() != wh) buf.resize(wh);
			if ((int)flags.size() != wh) flags.resize(wh);
			memset(&buf[0], 0x80, wh);
			memset(&flags[0], 1, wh);

			// stage a
#pragma omp parallel for
			for (int y = 0; y < h - 3; y++) {
				uint8_t const *src = GrayscaleLine(y);
				uint8_t *flags = &flags[w * y];
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

			{
				// stage b
				std::vector<Deinterlace::Task> tasks;
				for (int y = 0; y < h - 2; y += 2) {
					Deinterlace::Task task;
					task.y = y + 1;
					task.h = h;
					task.w = w;
					task.src0 = GrayscaleLine(y);
					task.src1 = GrayscaleLine(y + 2);
					tasks.push_back(task);
				}
				for (int y = 1; y < h - 2; y += 2) {
					Deinterlace::Task task;
					task.y = y + 1;
					task.h = h;
					task.w = w;
					task.src0 = GrayscaleLine(y);
					task.src1 = GrayscaleLine(y + 2);
					tasks.push_back(task);
				}
				const int span = 7;
				const int margin = span / 2;
				std::vector<uint8_t> buf0_(w + margin * 2);
				std::vector<uint8_t> buf1_(w + margin * 2);
				uint8_t *buf0 = buf0_.data();
				uint8_t *buf1 = buf1_.data();
#pragma omp parallel for
				for (size_t i = 0; i < tasks.size(); i++) {
					Deinterlace::Task const &task = tasks[i];
					const int w = task.w;
					int8_t *v = &buf[task.w * task.y];
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
			}

			newimage0 = image.copy();
			newimage1 = image.copy();

			{
				// stage c
				std::vector<Deinterlace::Task> tasks;
				for (int y = 0; y < h - 2; y += 2) {
					Deinterlace::Task task;
					task.y = y + 1;
					task.h = h;
					task.w = w;
					task.src0 = newimage0.scanLine(y);
					task.src1 = newimage0.scanLine(y + 2);
					task.dst = newimage0.scanLine(y + 1);
					tasks.push_back(task);
				}
				for (int y = 1; y < h - 2; y += 2) {
					Deinterlace::Task task;
					task.y = y + 1;
					task.h = h;
					task.w = w;
					task.src0 = newimage1.scanLine(y);
					task.src1 = newimage1.scanLine(y + 2);
					task.dst = newimage1.scanLine(y + 1);
					tasks.push_back(task);
				}
#pragma omp parallel for
				for (size_t i = 0; i < tasks.size(); i++) {
					Deinterlace::Task const &task = tasks[i];
					const int w = task.w;
					int8_t *v = &buf[w * task.y];
					uint8_t const *f = &flags[w * task.y];
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
			}

			//
		}
	}
	return std::make_pair<QImage, QImage>(std::move(newimage0), std::move(newimage1));
}

