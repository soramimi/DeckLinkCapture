#include "ImageUtil.h"
#include <omp.h>

static inline uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

QImage ImageUtil::qimage(const Image &image)
{
	QImage newimage;
	int w = image.width();
	int h = image.height();
	if (w > 0 && h > 0) {
		Image srcimage;
		switch (image.format()) {
		case Image::Format::UYVY8:
		case Image::Format::YUYV8:
		case Image::Format::RGB8:
			newimage = QImage(w, h, QImage::Format_RGB888);
			srcimage = image.convertToFormat(Image::Format::YUYV8).convertToFormat(Image::Format::RGB8);
			break;
		case Image::Format::UINT8:
			newimage = QImage(w, h, QImage::Format_Grayscale8);
			srcimage = image;
			break;
		}
		int stride = srcimage.bytesPerLine();
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = newimage.scanLine(y);
			memcpy(d, s, stride);
		}
	}
	return newimage;
}

Image ImageUtil::image(const QImage &image, Image::Format format)
{
	if (image.format() == QImage::Format_RGB32) {
		return ImageUtil::image(image.convertToFormat(QImage::Format_RGB888), format);
	}
	int w = image.width();
	int h = image.height();
	if (format == Image::Format::YUYV8 || format == Image::Format::UYVY8) {
		const QImage srcimage = image.convertToFormat(QImage::Format_RGB888);
		Image newimage(w, h, Image::Format::RGB8);
		int stride = newimage.bytesPerLine();
		for (int y = 0; y < h; y++) {
			uint8_t const *s = srcimage.scanLine(y);
			uint8_t *d = newimage.scanLine(y);
			memcpy(d, s, stride);
		}
		return newimage.convertToFormat(format);
	} else {
		QImage::Format qf = QImage::Format_Invalid;
		switch (format) {
		case Image::Format::UINT8:
			qf = QImage::Format_Grayscale8;
			break;
		case Image::Format::RGB8:
			qf = QImage::Format_RGB888;
			break;
		}
		if (qf != QImage::Format_Invalid) {
			const QImage srcimage = image.convertToFormat(qf);
			Image newimage(w, h, format);
			int stride = newimage.bytesPerLine();
			for (int y = 0; y < h; y++) {
				uint8_t const *s = srcimage.scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				memcpy(d, s, stride);
			}
			return newimage;
		}
	}
	return {};
}
