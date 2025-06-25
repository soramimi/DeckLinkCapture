#include "Deinterlace.h"
#include <QElapsedTimer>
#include <cstdint>
#include <functional>
#include <memory>
#include <omp.h>
#include <string.h>
#include <vector>

#ifdef _MSC_VER
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace {

inline int diff(int a, int b)
{
	int d = a - b;
	return d * d;
}

void process_row(int w, int stride, const uint8_t *prev, const uint8_t *curr, const uint8_t *next, uint8_t *dst, bool multiframe, bool alternateframe)
{
	uint16_t *tmpbuf = (uint16_t *)alloca(sizeof(uint16_t) * w * 5);
	uint16_t *scoreN2 = tmpbuf + w * 0; // -2
	uint16_t *scoreN1 = tmpbuf + w * 1; // -1
	uint16_t *score_0 = tmpbuf + w * 2; //  0
	uint16_t *scoreP1 = tmpbuf + w * 3; // +1
	uint16_t *scoreP2 = tmpbuf + w * 4; // +2

	auto CalcScore = [&](uint16_t *score, int shift){
		uint8_t const *a = curr + 2 - stride + shift;
		uint8_t const *b = curr + 2 + stride - shift;
		uint16_t u = 0;
		uint16_t v = diff(a[0], b[0]);
		for (int x = 0; x < w - 4; x++) {
			uint16_t t = u;
			u = v;
			v = diff(a[x + 1], b[x + 1]);
			score[x + 2] = t + u + v;
		}
	};
	CalcScore(scoreN2, -2);
	CalcScore(scoreN1, -1);
	CalcScore(score_0,  0);
	CalcScore(scoreP1, +1);
	CalcScore(scoreP2, +2);

	const int stride2 = stride * 2;

	uint8_t const *prevframe = !alternateframe ? prev : curr;
	uint8_t const *nextframe = !alternateframe ? curr : next;

	for (int x = 2; x < w - 2; x++) {

		uint16_t score = score_0[x];
		int shift = 0;
		if (score > scoreN1[x] || score > scoreP1[x]) {
			if (scoreN1[x] < scoreP1[x]) {
				score = scoreN1[x];
				shift = -1;
			} else {
				score = scoreP1[x];
				shift = +1;
			}
		}
		if (score > scoreN2[x] || score > scoreP2[x]) {
			if (scoreN2[x] < scoreP2[x]) {
				score = scoreN2[x];
				shift = -2;
			} else {
				score = scoreP2[x];
				shift = +2;
			}
		}

		int value = (curr[x - stride + shift] + curr[x + stride - shift]) / 2;

		if (multiframe) {
			const int a = curr[x - stride];
			const int b = curr[x + stride];
			const int c = diff(prev[x], next[x]) / 2;
			const int d = diff(prev[x - stride], a) + diff(prev[x + stride], b) / 2;
			const int e = diff(next[x - stride], a) + diff(next[x + stride], b) / 2;
			const int f = (prevframe[x] + nextframe[x]) / 2;
			int g = (prevframe[x - stride2] + nextframe[x - stride2]) / 2 - a;
			int h = (prevframe[x + stride2] + nextframe[x + stride2]) / 2 - b;
			if (g > h) { std::swap(g, h); }
			const int i = +std::min(std::min(f - b, f - a), g);
			const int j = -std::max(std::max(f - b, f - a), h);
			const int k = std::max(std::max(c, std::max(d, e)), std::max(i, j));
			const int min = f - k;
			const int max = f + k;
			value = std::min(std::max(value, min), max);
		}

		dst[x] = value;
	}
	dst[0] = dst[1] = dst[2];
	dst[w - 2] = dst[w - 1] = dst[w - 3];
}

void process_channel(int w, int h, int stride, const uint8_t *src_prev, const uint8_t *src_curr, const uint8_t *src_next, uint8_t *dst)
{
#pragma omp parallel for
	for (int y = 0; y < h; y++) {
		uint8_t *d = dst + stride * y;
		uint8_t const *curr = src_curr + stride * y;
		uint8_t const *prev = src_prev + stride * y;
		uint8_t const *next = src_next + stride * y;
		if ((y & 1) && y >= 1 && y < h - 1) {
			bool multiframe = (y >= 2 && y < h - 2);
			bool alternateframe = false;
			process_row(w, stride, prev, curr, next, d, multiframe, alternateframe);
		} else {
			memcpy(d, curr, w);
		}
	}
}

struct DeintRGB {
	Image::Format format;
	int w;
	int h;
	int w4;
	int h3;
	int stride;
	int offsetR;
	int offsetG;
	int offsetB;

	Image prepare(Image const &input)
	{
		format = input.format();
		w = input.width();
		h = input.height();
		w4 = w + 4;
		h3 = h * 3;
		stride = w4;
		offsetR = stride * h * 0;
		offsetG = stride * h * 1;
		offsetB = stride * h * 2;
		Image planar(w4, h3, Image::Format::UINT8);
		for (int y = 0; y < h; y++) {
			uint8_t const *s = input.scanLine(y);
			uint8_t *dR = planar.bits() + offsetR + stride * y;
			uint8_t *dG = planar.bits() + offsetG + stride * y;
			uint8_t *dB = planar.bits() + offsetB + stride * y;
			for (int x = 0; x < w; x++) {
				dR[2 + x] = s[3 * x + 0];
				dG[2 + x] = s[3 * x + 1];
				dB[2 + x] = s[3 * x + 2];
			}
			dR[0] = dR[1] = dR[2];
			dG[0] = dG[1] = dG[2];
			dB[0] = dB[1] = dB[2];
			dR[w + 2] = dR[w + 3] = dR[w + 1];
			dG[w + 2] = dG[w + 3] = dG[w + 1];
			dB[w + 2] = dB[w + 3] = dB[w + 1];
		}
		return planar;
	}

	Image perform(Image const &prev, Image const &curr, Image const &next)
	{
		Image ret;
		auto ValidImage = [&](Image const &img){
			return img.width() == w4 && img.height() == h3 && img.format() == Image::Format::UINT8;
		};
		if (ValidImage(prev) && ValidImage(curr) && ValidImage(next)) {
			Image dest(w4, h3, Image::Format::UINT8);

			process_channel(w4, h, stride, prev.bits() + offsetR, curr.bits() + offsetR, next.bits() + offsetR, dest.bits() + offsetR);
			process_channel(w4, h, stride, prev.bits() + offsetG, curr.bits() + offsetG, next.bits() + offsetG, dest.bits() + offsetG);
			process_channel(w4, h, stride, prev.bits() + offsetB, curr.bits() + offsetB, next.bits() + offsetB, dest.bits() + offsetB);

			ret = Image(w, h, format);
			for (int y = 0; y < h; y++) {
				uint8_t const *sR = dest.bits() + offsetR + stride * y + 2;
				uint8_t const *sG = dest.bits() + offsetG + stride * y + 2;
				uint8_t const *sB = dest.bits() + offsetB + stride * y + 2;
				uint8_t *d = ret.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[3 * x + 0] = sR[x];
					d[3 * x + 1] = sG[x];
					d[3 * x + 2] = sB[x];
				}
			}
		}
		return ret;
	}
};

struct DeintYUV {
	Image::Format format;
	int w;
	int h;
	int w4;
	int h3;
	int stride;
	int offsetY;
	int offsetU;
	int offsetV;

	Image prepare(Image const &input)
	{
		format = input.format();
		w = input.width();
		h = input.height();
		w4 = w + 4;
		h3 = h * 3;
		stride = w4;
		offsetY = stride * h * 0;
		offsetU = stride * h * 1;
		offsetV = stride * h * 2;
		Image planar(w4, h3, Image::Format::UINT8);
		for (int y = 0; y < h; y++) {
			uint8_t const *s = input.scanLine(y);
			uint8_t *dY = planar.bits() + offsetY + stride * y;
			uint8_t *dU = planar.bits() + offsetU + stride * y;
			uint8_t *dV = planar.bits() + offsetV + stride * y;
			switch (format) {
			case Image::Format::YUYV8:
				for (int x = 0; x < w / 2; x++) {
					dY[2 + 2 * x + 0] = s[4 * x + 0];
					dU[2 + x]         = s[4 * x + 1];
					dY[2 + 2 * x + 1] = s[4 * x + 2];
					dV[2 + x]         = s[4 * x + 3];
				}
				break;
			case Image::Format::UYVY8:
				for (int x = 0; x < w / 2; x++) {
					dY[2 + 2 * x + 0] = s[4 * x + 1];
					dU[2 + x]         = s[4 * x + 0];
					dY[2 + 2 * x + 1] = s[4 * x + 3];
					dV[2 + x]         = s[4 * x + 2];
				}
				break;
			}
			dY[0] = dY[1] = dY[2];
			dU[0] = dU[1] = dU[2];
			dV[0] = dV[1] = dV[2];
			dY[w + 2] = dY[w + 3] = dY[w + 1];
			dU[w / 2 + 2] = dU[w / 2 + 3] = dU[w / 2 + 1];
			dV[w / 2 + 2] = dV[w / 2 + 3] = dV[w / 2 + 1];
		}
		return planar;
	}

	Image perform(Image const &prev, Image const &curr, Image const &next)
	{
		Image ret;
		auto ValidImage = [&](Image const &img){
			return img.width() == w4 && img.height() == h3 && img.format() == Image::Format::UINT8;
		};
		if (ValidImage(prev) && ValidImage(curr) && ValidImage(next)) {
			Image dest = curr.copy();

			process_channel(w4, h, stride, prev.bits() + offsetY, curr.bits() + offsetY, next.bits() + offsetY, dest.bits() + offsetY);

			ret = Image(w, h, format);
			for (int y = 0; y < h; y++) {
				uint8_t const *sY = dest.bits() + offsetY + stride * y + 2;
				uint8_t const *sU = dest.bits() + offsetU + stride * (y & ~1) + 2;
				uint8_t const *sV = dest.bits() + offsetV + stride * (y & ~1) + 2;
				uint8_t *d = ret.scanLine(y);
				switch (format) {
				case Image::Format::YUYV8:
					for (int x = 0; x < w / 2; x++) {
						d[4 * x + 0] = sY[2 * x + 0];
						d[4 * x + 1] = sU[x];
						d[4 * x + 2] = sY[2 * x + 1];
						d[4 * x + 3] = sV[x];
					}
					break;
				case Image::Format::UYVY8:
					for (int x = 0; x < w / 2; x++) {
						d[4 * x + 1] = sY[2 * x + 0];
						d[4 * x + 0] = sU[x];
						d[4 * x + 3] = sY[2 * x + 1];
						d[4 * x + 2] = sV[x];
					}
					break;
				}
			}
		}
		return ret;
	}
};

}

template <typename T> Image Deinterlace::process(Image const &input)
{
	const int w = input.width();
	const int h = input.height();
	if (w < 1 || h < 1) return {};

//	QElapsedTimer t;
//	t.start();

	T d;

	Image planar = d.prepare(input);

	int prev_i;
	int curr_i;
	int next_i;
	{
		std::lock_guard lock(mutex_);
		if (image_count_ < NBUF) {
			next_i = (image_index_ + image_count_) % NBUF;
			image_buffer_[next_i] = planar;
			image_count_++;
			if (image_count_ == 1) {
				next_i = (next_i + 1) % NBUF;
				image_buffer_[next_i] = planar;
				image_count_++;
			}
			if (image_count_ == 2) {
				next_i = (next_i + 1) % NBUF;
				image_buffer_[next_i] = planar;
				image_count_++;
			}
			curr_i = (next_i + NBUF - 1) % NBUF;
			prev_i = (next_i + NBUF - 2) % NBUF;
		} else {
			return {};
		}
	}

	Image const &prev = image_buffer_[prev_i];
	Image const &curr = image_buffer_[curr_i];
	Image const &next = image_buffer_[next_i];
	Image ret = d.perform(prev, curr, next);

//	qDebug() << t.elapsed();

	while (1) {
		std::unique_lock lock(mutex_);
		if (prev_i == image_index_) {
			image_index_ = (image_index_ + 1) % NBUF;
			image_count_--;
			cond_.notify_all();
			return ret;
		}
		cond_.wait(lock);
	}
}

Image Deinterlace::deinterlace(Image const &input)
{
	switch (input.format()) {
	case Image::Format::YUYV8:
	case Image::Format::UYVY8:
		return process<DeintYUV>(input);
	case Image::Format::RGB8:
		return process<DeintRGB>(input);
	default:
		return process<DeintRGB>(input.convertToFormat(Image::Format::RGB8));
	}
}
