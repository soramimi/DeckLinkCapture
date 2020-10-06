#include "ImageUtil.h"
#include <omp.h>

static inline uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

QImage ImageUtil::qimage(const Image &image)
{
	if (image.format() == Image::Format::UYVY8) {
		int w = image.width();
		int h = image.height();
		QImage newimage = QImage(w, h, QImage::Format_RGB888);
#pragma omp parallel for
		for (int y = 0; y < h; y++) {
			uint8_t const *s = image.scanLine(y);
			uint8_t *d = newimage.scanLine(y);
			uint8_t U, V;
			U = V = 0;
			for (int x = 0; x < w; x++) {
				uint8_t Y = s[1];
				if ((x & 1) == 0) {
					U = s[0];
					V = s[2];
				}
				int R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
				int G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
				int B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
				d[0] = clamp_uint8(R);
				d[1] = clamp_uint8(G);
				d[2] = clamp_uint8(B);
				s += 2;
				d += 3;
			}
		}
		return newimage;
	}
	if (image.format() == Image::Format::YUYV8) {
		int w = image.width();
		int h = image.height();
		QImage newimage = QImage(w, h, QImage::Format_RGB888);
		for (int y = 0; y < h; y++) {
			uint8_t const *s = image.scanLine(y);
			uint8_t *d = newimage.scanLine(y);
			uint8_t U, V;
			U = V = 0;
			for (int x = 0; x < w; x++) {
				uint8_t Y = s[0];
				if ((x & 1) == 0) {
					U = s[1];
					V = s[3];
				}
				int R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
				int G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
				int B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
				d[0] = clamp_uint8(R);
				d[1] = clamp_uint8(G);
				d[2] = clamp_uint8(B);
				s += 2;
				d += 3;
			}
		}
		return newimage;
	}
	if (image.format() == Image::Format::RGB8) {
		int w = image.width();
		int h = image.height();
		int stride = image.bytesPerLine();
		QImage newimage = QImage(w, h, QImage::Format_RGB888);
		for (int y = 0; y < h; y++) {
			uint8_t const *s = image.scanLine(y);
			uint8_t *d = newimage.scanLine(y);
			memcpy(d, s, stride);
		}
		return newimage;
	}
	if (image.format() == Image::Format::UINT8) {
		int w = image.width();
		int h = image.height();
		int stride = image.bytesPerLine();
		QImage newimage = QImage(w, h, QImage::Format_Grayscale8);
		for (int y = 0; y < h; y++) {
			uint8_t const *s = image.scanLine(y);
			uint8_t *d = newimage.scanLine(y);
			memcpy(d, s, stride);
		}
		return newimage;
	}
	return {};
}

Image ImageUtil::image(const QImage &image, Image::Format format)
{
	if (image.format() == QImage::Format_RGB32) {
		return ImageUtil::image(image.convertToFormat(QImage::Format_RGB888), format);
	}
	int w = image.width();
	int h = image.height();
	if (format == Image::Format::UINT8) {
		const QImage srcimage = image.convertToFormat(QImage::Format_Grayscale8);
		Image newimage(w, h, format);
		switch (image.format()) {
		case QImage::Format_RGB888:
			for (int y = 0; y < h; y++) {
				uint8_t const *s = image.scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				memcpy(d, s, newimage.bytesPerLine());
			}
			break;
		}
		return newimage;
	}
	if (format == Image::Format::RGB8) {
		Image newimage(w, h, format);
		switch (image.format()) {
		case QImage::Format_RGB888:
			for (int y = 0; y < h; y++) {
				uint8_t const *s = image.scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				memcpy(d, s, newimage.bytesPerLine());
			}
			break;
		}
		return newimage;
	}
	if (format == Image::Format::UYVY8) {
		Image newimage(w, h, format);
		switch (image.format()) {
		case QImage::Format_RGB888:
			for (int y = 0; y < h; y++) {
				uint8_t const *s = image.scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w / 2; x++) {
					d[1] = ( 263 * s[0] + 516 * s[1] + 100 * s[2]) / 1024 + 16;
					d[3] = ( 263 * s[3] + 516 * s[4] + 100 * s[5]) / 1024 + 16;
					int R = (s[0] + s[3]) / 2;
					int G = (s[1] + s[4]) / 2;
					int B = (s[2] + s[5]) / 2;
					int U = (-152 * R - 298 * G + 450 * B) / 1024 + 128;
					int V = ( 450 * R - 377 * G -  73 * B) / 1024 + 128;
					d[0] = U;
					d[2] = V;
					s += 6;
					d += 4;
				}
				if (w & 1) {
					d[1] = ( 263 * s[0] + 516 * s[1] + 100 * s[2]) / 1024 + 16;
					d[0] = (-152 * s[0] - 298 * s[1] + 450 * s[2]) / 1024 + 128;
				}
			}
			break;
		}
		return newimage;
	}
	if (format == Image::Format::YUYV8) {
		Image newimage(w, h, format);
		switch (image.format()) {
		case QImage::Format_RGB888:
			for (int y = 0; y < h; y++) {
				uint8_t const *s = image.scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w / 2; x++) {
					d[0] = ( 263 * s[0] + 516 * s[1] + 100 * s[2]) / 1024 + 16;
					d[2] = ( 263 * s[3] + 516 * s[4] + 100 * s[5]) / 1024 + 16;
					int R = (s[0] + s[3]) / 2;
					int G = (s[1] + s[4]) / 2;
					int B = (s[2] + s[5]) / 2;
					int U = (-152 * R - 298 * G + 450 * B) / 1024 + 128;
					int V = ( 450 * R - 377 * G -  73 * B) / 1024 + 128;
					d[1] = U;
					d[3] = V;
					s += 6;
					d += 4;
				}
				if (w & 1) {
					d[0] = ( 263 * s[0] + 516 * s[1] + 100 * s[2]) / 1024 + 16;
					d[1] = (-152 * s[0] - 298 * s[1] + 450 * s[2]) / 1024 + 128;
				}
			}
			break;
		}
		return newimage;
	}
	return {};
}
