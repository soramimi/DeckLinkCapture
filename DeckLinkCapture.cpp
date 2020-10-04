#include "DeckLinkCapture.h"
#include "Deinterlace.h"
#include "Image.h"
#include "MainWindow.h"
#include <QDebug>
#include <QElapsedTimer>
#include <stdint.h>
#include <deque>

//#ifdef USE_OPENCV
//#include <opencv2/imgproc/imgproc.hpp>
//#endif

static inline uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

struct DeckLinkCapture::Private {
	DeckLinkCaptureDelegate *mainwindow = nullptr;
	std::deque<Task> tasks;
	QMutex mutex;
	QWaitCondition waiter;
	Image next_image0;
	Image next_image1;
	DeinterlaceMode deinterlace = DeinterlaceMode::InterpolateEven;
	BMDPixelFormat pixel_format = bmdFormat8BitYUV;
	BMDFieldDominance field_dominance = bmdUnknownFieldDominance;
	Deinterlace di;
	std::deque<Image> frame_queue;
};

DeckLinkCapture::DeckLinkCapture(DeckLinkCaptureDelegate *mainwindow)
	: m(new Private)
{
	m->mainwindow = mainwindow;
}

DeckLinkCapture::~DeckLinkCapture()
{
	stop();
	delete m;
}

DeckLinkCaptureDelegate *DeckLinkCapture::delegate()
{
	return m->mainwindow;
}

BMDPixelFormat DeckLinkCapture::pixelFormat() const
{
	return m->pixel_format;
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

void DeckLinkCapture::newFrame_(const Image &image0, const Image &image1)
{
	{
		QMutexLocker lock(&m->mutex);
		while (m->frame_queue.size() > 2) {
			m->frame_queue.pop_front();
		}
		if (!image0.isNull()) m->frame_queue.push_back(image0);
		if (!image1.isNull()) m->frame_queue.push_back(image1);
	}
	emit newFrame();
}

void DeckLinkCapture::addDevice(IDeckLink *decklink)
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->addDevice(decklink);
}

void DeckLinkCapture::removeDevice(IDeckLink *decklink)
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->removeDevice(decklink);
}

void DeckLinkCapture::updateProfile(IDeckLinkProfile *newProfile)
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->updateProfile(newProfile);
}

void DeckLinkCapture::changeDisplayMode(BMDDisplayMode dispmode, double fps)
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->changeDisplayMode(dispmode, fps);
}

void DeckLinkCapture::videoFrameArrived(AncillaryDataStruct const *ancillary_data, HDRMetadataStruct const *hdr_metadata, bool signal_valid)
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->videoFrameArrived(ancillary_data, hdr_metadata, signal_valid);
}

void DeckLinkCapture::haltStreams()
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->haltStreams();
}

void DeckLinkCapture::criticalError(const QString &title, const QString &message)
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->criticalError(title, message);
}

void DeckLinkCapture::customEvent(QEvent *event)
{
	if (event->type() == kAddDeviceEvent) {
		DeckLinkDeviceDiscoveryEvent *discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		addDevice(discoveryEvent->decklink());
	} else if (event->type() == kRemoveDeviceEvent) {
		DeckLinkDeviceDiscoveryEvent *discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		removeDevice(discoveryEvent->decklink());
	} else if (event->type() == kVideoFormatChangedEvent) {
		DeckLinkInputFormatChangedEvent *formatEvent = dynamic_cast<DeckLinkInputFormatChangedEvent*>(event);
		changeDisplayMode(formatEvent->DisplayMode(), formatEvent->fps());
	} else if (event->type() == kVideoFrameArrivedEvent) {
		DeckLinkInputFrameArrivedEvent *frameArrivedEvent = dynamic_cast<DeckLinkInputFrameArrivedEvent*>(event);
		videoFrameArrived(frameArrivedEvent->AncillaryData(), frameArrivedEvent->HDRMetadata(), frameArrivedEvent->SignalValid());
	} else if (event->type() == kProfileActivatedEvent) {
		DeckLinkProfileCallbackEvent *profileChangedEvent = dynamic_cast<DeckLinkProfileCallbackEvent*>(event);
		updateProfile(profileChangedEvent->Profile());
	}
}

Image DeckLinkCapture::createImage(int w, int h, BMDPixelFormat pixel_format, uint8_t const *data, int size)
{
	switch (pixel_format) {
	case bmdFormat10BitRGB:
		if (w * h * 4 == size) {
			Image image(w, h, Image::Format::RGB8);
			for (int y = 0; y < h; y++) {
				uint8_t const *src = data + 4 * w * y;
				uint8_t *dst = image.scanLine(y);
				for (int x = 0; x < w; x++) {
					uint32_t t = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
					dst[0] = t >> 22;
					dst[1] = t >> 12;
					dst[2] = t >> 2;
					src += 4;
					dst += 3;
				}
			}
			return image;
		}
		break;
	case bmdFormat10BitRGBX:
		if (w * h * 4 == size) {
			Image image(w, h, Image::Format::RGB8);
			for (int y = 0; y < h; y++) {
				uint8_t const *src = data + 4 * w * y;
				uint8_t *dst = image.scanLine(y);
				for (int x = 0; x < w; x++) {
					uint32_t t = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
					dst[0] = t >> 24;
					dst[1] = t >> 14;
					dst[2] = t >> 4;
					src += 4;
					dst += 3;
				}
			}
			return image;
		}
		break;
	case bmdFormat10BitRGBXLE:
		if (w * h * 4 == size) {
			Image image(w, h, Image::Format::RGB8);
			for (int y = 0; y < h; y++) {
				uint8_t const *src = data + 4 * w * y;
				uint8_t *dst = image.scanLine(y);
				for (int x = 0; x < w; x++) {
					uint32_t t = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
					dst[0] = t >> 24;
					dst[1] = t >> 14;
					dst[2] = t >> 4;
					src += 4;
					dst += 3;
				}
			}
			return image;
		}
		break;
	case bmdFormat8BitBGRA:
		if (w * h * 4 == size) {
			Image image(w, h, Image::Format::RGB8);
			for (int y = 0; y < h; y++) {
				uint8_t const *src = data + 4 * w * y;
				uint8_t *dst = image.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[0] = src[2];
					dst[1] = src[1];
					dst[2] = src[0];
					src += 4;
					dst += 3;
				}
			}
			return image;
		}
		break;
	case bmdFormat8BitARGB:
		if (w * h * 4 == size) {
			Image image(w, h, Image::Format::RGB8);
			for (int y = 0; y < h; y++) {
				uint8_t const *src = data + 4 * w * y;
				uint8_t *dst = image.scanLine(y);
				for (int x = 0; x < w; x++) {
					dst[0] = src[1];
					dst[1] = src[2];
					dst[2] = src[3];
					src += 4;
					dst += 3;
				}
			}
			return image;
		}
		break;
	case bmdFormat8BitYUV:
	case bmdFormat10BitYUV:
		if (w * h * 2 == size) {
			Image image(w, h, Image::Format::UYUV8);
			for (int y = 0; y < h; y++) {
				uint8_t const *src = data + w * y * 2;
				uint8_t *dst = image.scanLine(y);
				memcpy(dst, src, image.bytesPerLine());
			}
			return image;
		}
		break;
	}
	return {};
}

void DeckLinkCapture::process(Task const &task)
{
	if (task.isValid()) {
		if (m->deinterlace == DeinterlaceMode::None || m->field_dominance == bmdProgressiveFrame) {
			newFrame_(task.image, {});
			goto done;
		}
		{
			Image image = task.image;
			const int w = image.width();
			const int h = image.height();
			Image bytes1(w, h, Image::Format::RGB8);
			const int stride = image.bytesPerLine();
			if (m->deinterlace == DeinterlaceMode::InterpolateEven) {
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
				newFrame_(bytes1, {});
				goto done;
			}
			if (m->deinterlace == DeinterlaceMode::InterpolateOdd) {
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
				newFrame_(bytes1, {});
				goto done;
			}
			if (m->deinterlace == DeinterlaceMode::Merge || m->deinterlace == DeinterlaceMode::MergeX2) {
				const bool x2frame = (m->deinterlace == DeinterlaceMode::MergeX2);
				Image image0;
				Image image1;
				{
					QMutexLocker lock(&m->mutex);
					image0 = m->next_image0;
					image1 = m->next_image1;
				}
				if (m->field_dominance == bmdUpperFieldFirst) {
					newFrame_(image0, x2frame ? image1 : Image());
				} else if (m->field_dominance == bmdLowerFieldFirst) {
					newFrame_(image1, x2frame ? image0 : Image());
				} else {
					newFrame_(image1, {});
				}
				auto pair = m->di.filter(image);
				{
					QMutexLocker lock(&m->mutex);
					m->next_image0 = pair.first;
					m->next_image1 = pair.second;
				}
				goto done;
			}
		}
done:;
	}
}

void DeckLinkCapture::run()
{
	while (1) {
		Task task;
		{
			QMutexLocker lock(&m->mutex);
			if (isInterruptionRequested()) break;
			if (m->tasks.empty()) {
				m->waiter.wait(&m->mutex);
			}
			if (!m->tasks.empty()) {
				task = m->tasks.front();
				m->tasks.pop_front();
			}
		}
		if (task.isValid()) {
			process(task);
		}
	}
}

void DeckLinkCapture::pushFrame(Task const &task)
{
	QMutexLocker lock(&m->mutex);
	while (m->tasks.size() > 2) {
		m->tasks.pop_front();
	}
	m->tasks.push_back(task);
	m->waiter.wakeAll();
}

void DeckLinkCapture::clear()
{
	QMutexLocker lock(&m->mutex);
	m->next_image0 = {};
	m->next_image1 = {};
}

bool DeckLinkCapture::startCapture(DeckLinkInputDevice *selectedDevice, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio)
{
	clear();
	if (selectedDevice) {
		if (selectedDevice->startCapture(displayMode, nullptr, applyDetectedInputMode, input_audio)) {
			m->field_dominance = fieldDominance;
			return true;
		}
	}
	return false;
}

void DeckLinkCapture::stop()
{
	{
		QMutexLocker lock(&m->mutex);
		m->tasks.clear();
		requestInterruption();
		m->waiter.wakeAll();
	}

	clear();

	if (!wait(300)) {
		terminate();
	}
}

Image DeckLinkCapture::nextFrame()
{
	Image image;
	QMutexLocker lock(&m->mutex);
	if (!m->frame_queue.empty()) {
		image = m->frame_queue.front();
		m->frame_queue.pop_front();
	}
	return image;
}

