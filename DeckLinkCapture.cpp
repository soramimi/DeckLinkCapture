#include "DeckLinkCapture.h"
#include "DeckLinkDeviceDiscovery.h"
#include "ProfileCallback.h"
#include "common.h"

static inline uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

struct DeckLinkCapture::Private {
	DeckLinkCaptureDelegate *mainwindow = nullptr;
	BMDPixelFormat pixel_format = bmdFormat8BitYUV;
	BMDFieldDominance field_dominance = bmdUnknownFieldDominance;
};

DeckLinkCapture::DeckLinkCapture(DeckLinkCaptureDelegate *mainwindow)
	: m(new Private)
{
	m->mainwindow = mainwindow;
}

DeckLinkCapture::~DeckLinkCapture()
{
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

void DeckLinkCapture::changeDisplayMode(BMDDisplayMode dispmode, Rational const &fps)
{
	Q_ASSERT(m->mainwindow);
	m->mainwindow->changeDisplayMode(dispmode, fps);
}

//void DeckLinkCapture::videoFrameArrived(AncillaryDataStruct const *ancillary_data, HDRMetadataStruct const *hdr_metadata, bool signal_valid)
//{
//	Q_ASSERT(m->mainwindow);
//	m->mainwindow->videoFrameArrived(ancillary_data, hdr_metadata, signal_valid);
//}

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
//	} else if (event->type() == kVideoFrameArrivedEvent) {
//		DeckLinkInputFrameArrivedEvent *frameArrivedEvent = dynamic_cast<DeckLinkInputFrameArrivedEvent*>(event);
//		videoFrameArrived(frameArrivedEvent->AncillaryData(), frameArrivedEvent->HDRMetadata(), frameArrivedEvent->SignalValid());
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
		if (w * h * 2 == size) {
			Image image(w, h, Image::Format::UYVY8);
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

bool DeckLinkCapture::startCapture(DeckLinkInputDevice *selectedDevice, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio)
{
	if (selectedDevice) {
		if (selectedDevice->startCapture(displayMode, nullptr, applyDetectedInputMode, input_audio)) {
			m->field_dominance = fieldDominance;
			return true;
		}
	}
	return false;
}

