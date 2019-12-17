#include "DeckLinkCapture.h"
#include "Deinterlace.h"
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
	QImage next_image0;
	QImage next_image1;
	DeinterlaceMode deinterlace = DeinterlaceMode::InterpolateEven;
	BMDPixelFormat pixel_format = bmdFormat8BitYUV;
	BMDFieldDominance field_dominance = bmdUnknownFieldDominance;
	Deinterlace di;
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
	clear();
	m->deinterlace = mode;
}

DeinterlaceMode DeckLinkCapture::deinterlaceMode() const
{
	if (m->field_dominance == bmdProgressiveFrame) {
		return DeinterlaceMode::None;
	}
	return m->deinterlace;
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

		if (image.width() == w && image.height() == h) {
			int stride = w * 3;

			QImage bytes1(w, h, QImage::Format_RGB888);

			if (m->deinterlace == DeinterlaceMode::None || m->field_dominance == bmdProgressiveFrame) {
				emit newFrame(image, QImage());
			} else if (m->deinterlace == DeinterlaceMode::InterpolateEven) {
				uint8_t const *s = (uint8_t const *)image.scanLine(0);
				uint8_t *d0 = (uint8_t *)bytes1.scanLine(0);
				memcpy(bytes1.scanLine(0), s, stride);
				int y = 1;
				while (y + 1 < h) {
					s = (uint8_t const *)image.scanLine(y + 1);
					uint8_t *d1 = d0 + stride;
					uint8_t *d2 = d0 + stride * 2;
					memcpy(d2, s, stride);
					for (int x = 0; x < w; x++) {
						int i = x * 3;
						int j = i + 1;
						int k = i + 2;
						d1[i] = (d0[i] + d2[i]) / 2;
						d1[j] = (d0[j] + d2[j]) / 2;
						d1[k] = (d0[k] + d2[k]) / 2;
					}
					d0 = d2;
					y += 2;
				}
				memcpy((uint8_t *)bytes1.scanLine(h - 1), d0, stride);
				emit newFrame(bytes1, QImage());
			} else if (m->deinterlace == DeinterlaceMode::InterpolateOdd) {
				uint8_t const *s = (uint8_t const *)image.scanLine(1);
				uint8_t *d0 = (uint8_t *)bytes1.scanLine(0);
				memcpy(d0, s, stride);
				d0 += stride;
				memcpy(d0, s, stride);
				int y = 3;
				while (y < h) {
					s = (uint8_t const *)image.scanLine(y);
					uint8_t *d1 = d0 + stride;
					uint8_t *d2 = d0 + stride * 2;
					memcpy(d2, s, stride);
					for (int x = 0; x < w; x++) {
						int i = x * 3;
						int j = i + 1;
						int k = i + 2;
						d1[i] = (d0[i] + d2[i]) / 2;
						d1[j] = (d0[j] + d2[j]) / 2;
						d1[k] = (d0[k] + d2[k]) / 2;
					}
					d0 = d2;
					y += 2;
				}
				emit newFrame(bytes1, QImage());
			} else if (m->deinterlace == DeinterlaceMode::Merge || m->deinterlace == DeinterlaceMode::MergeX2) {
				const bool x2frame = (m->deinterlace == DeinterlaceMode::MergeX2);
				QImage image0;
				QImage image1;
				{
					QMutexLocker lock(&m->mutex);
					image0 = m->next_image0;
					image1 = m->next_image1;
				}
				if (m->field_dominance == bmdUpperFieldFirst) {
					emit newFrame(image0, x2frame ? image1 : QImage());
				} else if (m->field_dominance == bmdLowerFieldFirst) {
					emit newFrame(image1, x2frame ? image0 : QImage());
				} else {
					emit newFrame(image1, QImage());
				}
				auto pair = m->di.filter(image);
				{
					QMutexLocker lock(&m->mutex);
					m->next_image0 = pair.first;
					m->next_image1 = pair.second;
				}
			} else {
				emit newFrame(image, QImage());
			}
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

void DeckLinkCapture::clear()
{
	QMutexLocker lock(&m->mutex);
	m->next_image0 = QImage();
	m->next_image1 = QImage();
}

bool DeckLinkCapture::start(DeckLinkInputDevice *selectedDevice, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio)
{
	clear();
	if (selectedDevice) {
		m->di.start();
		if (selectedDevice->startCapture(displayMode, this, applyDetectedInputMode, input_audio)) {
			m->field_dominance = fieldDominance;
			QThread::start();
			return true;
		}
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

	m->di.stop();
	clear();

	if (!wait(300)) {
		terminate();
	}
}

