#include "DeckLinkCapture.h"
#include <QDebug>
#include <QElapsedTimer>
#include <stdint.h>

#ifdef USE_OPENCV
#include <opencv2/imgproc/imgproc.hpp>
#endif

static inline uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

struct DeckLinkCapture::Private {
	Task task;
	QMutex mutex;
	QWaitCondition waiter;
	QSize image_size;
	QImage last_image;
	QImage curr_image;
	DeinterlaceMode deinterlace = DeinterlaceMode::InterpolateEven;
	BMDPixelFormat pixel_format = bmdFormat8BitYUV;
};

DeckLinkCapture::DeckLinkCapture()
	: m(new Private)
{
}

DeckLinkCapture::~DeckLinkCapture()
{
	stop();
	delete m;
}

void DeckLinkCapture::setPixelFormat(BMDPixelFormat pixel_format)
{
	m->pixel_format = pixel_format;
}

void DeckLinkCapture::setDeinterlaceMode(DeinterlaceMode mode)
{
	m->deinterlace = mode;
}

HRESULT DeckLinkCapture::DrawFrame(IDeckLinkVideoFrame *frame)
{
	if (frame) {
		int w = frame->GetWidth();
		int h = frame->GetHeight();
		int stride = frame->GetRowBytes();
		uint8_t const *bytes = nullptr;
		if (w > 0 && h > 0 && frame->GetBytes((void **)&bytes) == S_OK && bytes) {
			m->image_size = QSize(w, h);
			Task t;
			t.sz = QSize(w, h);
			t.ba = QByteArray((char const *)bytes, stride * h);
			pushFrame(t);
		}
	}
	return S_OK;
}

void DeckLinkCapture::process(Task const &task)
{
	int w = task.sz.width();
	int h = task.sz.height();
	if (w > 0 && h > 0 && !task.ba.isEmpty()) {
		QElapsedTimer t;
		t.start();

		QImage image;

		if (m->pixel_format == bmdFormat10BitRGB) {
			if (w * h * 4 == task.ba.size()) {
				image = QImage(w, h, QImage::Format_RGB888);
				void const *data = task.ba.data();
				int stride = w * 4;
				for (int y = 0; y < h; y++) {
					uint8_t const *src = (uint8_t const *)data + stride * y;
					uint8_t *dst = image.scanLine(y);
					for (int x = 0; x < w; x++) {
						uint32_t t = (src[x * 4 + 0] << 24) | (src[x * 4 + 1] << 16) | (src[x * 4 + 2] << 8) | src[x * 4 + 3];
						dst[x * 3 + 0] = t >> 22;
						dst[x * 3 + 1] = t >> 12;
						dst[x * 3 + 2] = t >> 2;
					}
				}
			}
		} else if (m->pixel_format == bmdFormat10BitRGBX) {
			if (w * h * 4 == task.ba.size()) {
				image = QImage(w, h, QImage::Format_RGB888);
				void const *data = task.ba.data();
				int stride = w * 4;
				for (int y = 0; y < h; y++) {
					uint8_t const *src = (uint8_t const *)data + stride * y;
					uint8_t *dst = image.scanLine(y);
					for (int x = 0; x < w; x++) {
						uint32_t t = (src[x * 4 + 0] << 24) | (src[x * 4 + 1] << 16) | (src[x * 4 + 2] << 8) | src[x * 4 + 3];
						dst[x * 3 + 0] = t >> 24;
						dst[x * 3 + 1] = t >> 14;
						dst[x * 3 + 2] = t >> 4;
					}
				}
			}
		} else if (m->pixel_format == bmdFormat10BitRGBXLE) {
			if (w * h * 4 == task.ba.size()) {
				image = QImage(w, h, QImage::Format_RGB888);
				void const *data = task.ba.data();
				int stride = w * 4;
				for (int y = 0; y < h; y++) {
					uint8_t const *src = (uint8_t const *)data + stride * y;
					uint8_t *dst = image.scanLine(y);
					for (int x = 0; x < w; x++) {
						uint32_t t = (src[x * 4 + 3] << 24) | (src[x * 4 + 2] << 16) | (src[x * 4 + 1] << 8) | src[x * 4 + 0];
						dst[x * 3 + 0] = t >> 24;
						dst[x * 3 + 1] = t >> 14;
						dst[x * 3 + 2] = t >> 4;
					}
				}
			}
		} else if (m->pixel_format == bmdFormat8BitBGRA) {
			if (w * h * 4 == task.ba.size()) {
				image = QImage(w, h, QImage::Format_RGB888);
				void const *data = task.ba.data();
				int stride = w * 4;
				for (int y = 0; y < h; y++) {
					uint8_t const *src = (uint8_t const *)data + stride * y;
					uint8_t *dst = image.scanLine(y);
					for (int x = 0; x < w; x++) {
						dst[x * 3 + 0] = src[x * 4 + 2];
						dst[x * 3 + 1] = src[x * 4 + 1];
						dst[x * 3 + 2] = src[x * 4 + 0];
					}
				}
			}
		} else if (m->pixel_format == bmdFormat8BitARGB) {
			if (w * h * 4 == task.ba.size()) {
				image = QImage(w, h, QImage::Format_RGB888);
				void const *data = task.ba.data();
				int stride = w * 4;
				for (int y = 0; y < h; y++) {
					uint8_t const *src = (uint8_t const *)data + stride * y;
					uint8_t *dst = image.scanLine(y);
					for (int x = 0; x < w; x++) {
						dst[x * 3 + 0] = src[x * 4 + 1];
						dst[x * 3 + 1] = src[x * 4 + 2];
						dst[x * 3 + 2] = src[x * 4 + 3];
					}
				}
			}
		} else if (m->pixel_format == bmdFormat8BitYUV || m->pixel_format == bmdFormat10BitYUV) {
			image = QImage(w, h, QImage::Format_RGB888);
#ifdef USE_OPENCV
			cv::Mat mat_yuyv(h, w, CV_8UC2, const_cast<void *>((void const *)task.ba.data()));
			cv::Mat mat_rgb;
			cv::cvtColor(mat_yuyv, mat_rgb, cv::COLOR_YUV2RGB_UYVY);
			memcpy(image.bits(), mat_rgb.ptr(0), w * h * 3);
#else
			for (int y = 0; y < h; y++) {
				uint8_t const *src = (uint8_t const *)task.ba.data() + w * y * 2;
				uint8_t *dst = image.scanLine(y);
				uint8_t u, v;
				u = v = 0;
				for (int x = 0; x < w; x++) {
					uint8_t y = src[1];
					if ((x & 1) == 0) {
						u = src[0];
						v = src[2];
					}
					int r = ((y - 16) * 1192 +                    (v - 128) * 1634) / 1024;
					int g = ((y - 16) * 1192 - (u - 128) * 400  - (v - 128) * 832 ) / 1024;
					int b = ((y - 16) * 1192 + (u - 128) * 2065                   ) / 1024;
					dst[0] = clamp_uint8(r);
					dst[1] = clamp_uint8(g);
					dst[2] = clamp_uint8(b);
					src += 2;
					dst += 3;
				}
			}
#endif
		}
		m->last_image = m->curr_image;
		m->curr_image = image;
		if (m->last_image.width() == w && m->last_image.height() == h && m->curr_image.width() == w && m->curr_image.height() == h) {
			int stride = w * 3;

			QImage bytes1(w, h, QImage::Format_RGB888);
			QImage bytes2(w, h, QImage::Format_RGB888);

			if (m->deinterlace == DeinterlaceMode::None) {
				image = m->curr_image;
			} else if (m->deinterlace == DeinterlaceMode::InterpolateEven) {
				for (int y = 0; y + 2 < h; y += 2) {
					uint8_t const *s = (uint8_t const *)m->last_image.scanLine(y);
					uint8_t *d0 = (uint8_t *)bytes1.scanLine(y);
					if (y == 0) memcpy(d0, s, stride);
					uint8_t *d1 = d0 + stride;
					uint8_t *d2 = d0 + stride * 2;
					memcpy(d2, s, stride);
					for (int x = 0; x < w; x++) {
						int i = x * 3;
						d1[i] = (d0[i] + d2[i]) / 2;
						i++;
						d1[i] = (d0[i] + d2[i]) / 2;
						i++;
						d1[i] = (d0[i] + d2[i]) / 2;
					}
				}
				image = bytes1;
			} else if (m->deinterlace == DeinterlaceMode::InterpolateOdd) {
				for (int y = 0; y + 2 < h; y += 2) {
					uint8_t const *s = (uint8_t const *)m->last_image.scanLine(y + 1);
					uint8_t *d0 = (uint8_t *)bytes1.scanLine(y);
					if (y == 0) memcpy(d0, s, stride);
					uint8_t *d1 = d0 + stride;
					uint8_t *d2 = d0 + stride * 2;
					memcpy(d2, s, stride);
					for (int x = 0; x < w; x++) {
						int i = x * 3;
						d1[i] = (d0[i] + d2[i]) / 2;
						i++;
						d1[i] = (d0[i] + d2[i]) / 2;
						i++;
						d1[i] = (d0[i] + d2[i]) / 2;
					}
				}
				image = bytes1;
			} else if (m->deinterlace == DeinterlaceMode::Merge) {
				for (int y = 0; y + 1 < h; y += 2) {
					uint8_t const *s1 = (uint8_t const *)m->last_image.scanLine(y);
					uint8_t const *s2 = (uint8_t const *)m->curr_image.scanLine(y);
					uint8_t *d1 = (uint8_t *)bytes1.scanLine(y);
					uint8_t *d2 = (uint8_t *)bytes2.scanLine(y);
					memcpy(d1, s1, stride);
					memcpy(d1 + stride, s2 + stride, stride);
					memcpy(d2, s2, stride);
					memcpy(d2 + stride, s1 + stride, stride);
				}
				unsigned int diff0 = 0;
				unsigned int diff1 = 0;
				unsigned int diff2 = 0;
				int x0 = w / 4;
				int x1 = w - x0;
				int y0 = h / 4;
				int y1 = h - y0;
				for (int y = y0; y < y1; y++) {
					uint8_t const *p0 = (uint8_t const *)m->curr_image.scanLine(y);
					uint8_t const *p1 = (uint8_t const *)bytes1.scanLine(y);
					uint8_t const *p2 = (uint8_t const *)bytes2.scanLine(y);
					for (int x = x0; x < x1; x++) {
						int d0 = (int)p0[x * 3 + 1] - (int)p0[x * 3 + 1 + stride];
						int d1 = (int)p1[x * 3 + 1] - (int)p1[x * 3 + 1 + stride];
						int d2 = (int)p2[x * 3 + 1] - (int)p2[x * 3 + 1 + stride];
						diff0 += d0 * d0;
						diff1 += d1 * d1;
						diff2 += d2 * d2;
					}
				}
				if (diff0 < diff1 && diff0 < diff2) {
					image = m->curr_image;
				} else if (diff1 < diff2) {
					image = bytes1;
				} else {
					image = bytes2;
				}
			} else {
				image = m->curr_image;
			}

			emit newFrame(image);
		}
		qDebug() << QString("%1ms").arg(t.elapsed());
	}
}

void DeckLinkCapture::run()
{
	while (!isInterruptionRequested()) {
		Task task;
		{
			QMutexLocker lock(&m->mutex);
			if (m->task.ba.isEmpty()) {
				m->waiter.wait(&m->mutex);
				continue;
			}
			std::swap(task, m->task);
		}
		process(task);
	}
}

void DeckLinkCapture::pushFrame(Task const &task)
{
	QMutexLocker lock(&m->mutex);
	if (!m->task.ba.isEmpty()) {
		qDebug() << "frame dropped";
	}
	m->task = task;
	m->waiter.wakeAll();
}

bool DeckLinkCapture::start(DeckLinkInputDevice *selectedDevice, BMDDisplayMode displayMode, bool applyDetectedInputMode, bool input_audio)
{
	if (selectedDevice->startCapture(displayMode, this, applyDetectedInputMode, input_audio)) {
		QThread::start();
		return true;
	}
	return false;
}

void DeckLinkCapture::stop()
{
	{
		QMutexLocker lock(&m->mutex);
		m->task = Task();
		requestInterruption();
		m->waiter.wakeAll();
	}
	if (!wait(300)) {
		terminate();
	}
}

