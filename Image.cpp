#include "Image.h"
#include <stdint.h>
#include <algorithm>

static inline uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

Image Image::convertToFormat(Image::Format dformat) const
{
	Image::Format sformat = format();

	if (sformat == dformat) return *this;

	const int w = width();
	const int h = height();

	if ((sformat == Format::UYVY8 && dformat == Format::YUYV8) || (sformat == Format::YUYV8 && dformat == Format::UYVY8)) {
		Image newimage(w, h, dformat);
		for (int y = 0; y < h; y++) {
			uint8_t const *s = scanLine(y);
			uint8_t *d = newimage.scanLine(y);
			for (int x = 0; x < w; x++) {
				d[0] = s[1];
				d[1] = s[0];
				s += 2;
				d += 2;
			}
		}
		return newimage;
	}

	if (dformat == Format::RGB8) {
		if (sformat == Format::UYVY8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
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
		if (sformat == Format::YUYV8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
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
		if (sformat == Format::UINT8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[0] = d[1] = d[2] = *s++;
					d += 3;
				}
			}
			return newimage;
		}
	}

	if (dformat == Format::UINT8) {
		if (sformat == Format::RGB8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w; x++) {
					int R = s[0];
					int G = s[1];
					int B = s[2];
					*d++ = (263 * R + 516 * G + 100 * B) / 1024 + 16;
					s += 3;
				}
			}
			return newimage;
		}
		if (sformat == Format::UYVY8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w; x++) {
					*d++ = s[1];
					s += 2;
				}
			}
			return newimage;
		}
		if (sformat == Format::YUYV8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w; x++) {
					*d++ = s[0];
					s += 2;
				}
			}
			return newimage;
		}
	}

	if (dformat == Format::UYVY8) {
		if (sformat == Format::RGB8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
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
					int R = s[0];
					int G = s[1];
					int B = s[2];
					int U = (-152 * R - 298 * G + 450 * B) / 1024 + 128;
					d[0] = U;
				}
			}
			return newimage;
		}
		if (sformat == Format::UINT8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[1] = *s++;
					d[0] = 128;
					d += 2;
				}
			}
			return newimage;
		}
	}

	if (dformat == Format::YUYV8) {
		if (sformat == Format::RGB8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
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
					int R = s[0];
					int G = s[1];
					int B = s[2];
					int U = (-152 * R - 298 * G + 450 * B) / 1024 + 128;
					d[1] = U;
				}
			}
			return newimage;
		}
		if (sformat == Format::UINT8) {
			Image newimage(w, h, dformat);
			for (int y = 0; y < h; y++) {
				uint8_t const *s = scanLine(y);
				uint8_t *d = newimage.scanLine(y);
				for (int x = 0; x < w; x++) {
					d[0] = *s++;
					d[1] = 128;
					d += 2;
				}
			}
			return newimage;
		}
	}

	return {};
}
