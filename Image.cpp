#include "Image.h"
#include "GlobalData.h"
#include <cstdint>
#include <algorithm>
#include <QElapsedTimer>
#include <QDebug>
#include "CudaPlugin/src/Cuda.h"

static inline uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static inline uint8_t gray(int r, int g, int b)
{
	return (r * 306 + g * 601 + b * 117) / 1024;
}



Image Image::convertToFormat(Image::Format dformat) const
{
	Image::Format sformat = format();

	if (sformat == dformat) return *this;

	const int w = width();
	const int h = height();

	bool cuda = (bool)global->cuda_plugin;

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
			if (cuda) {
				global->cuda_plugin->convert_uyvy_to_rgb(w, h, this->bits(), newimage.bits());
			} else {
				for (int y = 0; y < h; y++) {
					uint8_t const *s = scanLine(y);
					uint8_t *d = newimage.scanLine(y);
					uint8_t Y, U, V;
					U = V = 0;
					int w2 = w / 2;
					for (int x = 0; x < w2; x++) {
						int R, G, B;
						U = s[0];
						V = s[2];
						Y = s[1];
						R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
						G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
						B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
						d[0] = clamp_uint8(R);
						d[1] = clamp_uint8(G);
						d[2] = clamp_uint8(B);
						Y = s[3];
						R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
						G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
						B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
						d[3] = clamp_uint8(R);
						d[4] = clamp_uint8(G);
						d[5] = clamp_uint8(B);
						s += 4;
						d += 6;
					}
					if (w & 1) {
						int U = s[0];
						int Y = s[1];
						int R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
						int G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
						int B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
						d[0] = clamp_uint8(R);
						d[1] = clamp_uint8(G);
						d[2] = clamp_uint8(B);
					}
				}
			}
			return newimage;
		}
		if (sformat == Format::YUYV8) {
			Image newimage(w, h, dformat);
			if (cuda) {
				global->cuda_plugin->convert_yuyv_to_rgb(w, h, this->bits(), newimage.bits());
			} else {
				for (int y = 0; y < h; y++) {
					uint8_t const *s = scanLine(y);
					uint8_t *d = newimage.scanLine(y);
					uint8_t Y, U, V;
					U = V = 0;
					int w2 = w / 2;
					for (int x = 0; x < w2; x++) {
						int R, G, B;
						U = s[1];
						V = s[3];
						Y = s[0];
						R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
						G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
						B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
						d[0] = clamp_uint8(R);
						d[1] = clamp_uint8(G);
						d[2] = clamp_uint8(B);
						Y = s[2];
						R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
						G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
						B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
						d[3] = clamp_uint8(R);
						d[4] = clamp_uint8(G);
						d[5] = clamp_uint8(B);
						s += 4;
						d += 6;
					}
					if (w & 1) {
						int U = s[1];
						int Y = s[0];
						int R = ((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024;
						int G = ((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
						int B = ((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
						d[0] = clamp_uint8(R);
						d[1] = clamp_uint8(G);
						d[2] = clamp_uint8(B);
					}
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
					*d++ = gray(R, G, B);
					s += 3;
				}
			}
			return newimage;
		}
		if (sformat == Format::UYVY8) {
			Image newimage(w, h, dformat);
			if (cuda) {
				global->cuda_plugin->convert_uyvy_to_gray(w, h, this->bits(), newimage.bits());
			} else {
				for (int y = 0; y < h; y++) {
					uint8_t const *s = scanLine(y);
					uint8_t *d = newimage.scanLine(y);
					uint8_t Y, U, V;
					U = V = 0;
					int w2 = w / 2;
					for (int x = 0; x < w2; x++) {
						int R, G, B;
						U = s[0];
						V = s[2];
						Y = s[1];
						R = clamp_uint8(((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024);
						G = clamp_uint8(((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024);
						B = clamp_uint8(((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024);
						*d++ = gray(R, G, B);
						Y = s[3];
						R = clamp_uint8(((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024);
						G = clamp_uint8(((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024);
						B = clamp_uint8(((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024);
						*d++ = gray(R, G, B);
						s += 4;
					}
					if (w & 1) {
						U = s[0];
						Y = s[1];
						int R = clamp_uint8(((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024);
						int G = clamp_uint8(((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024);
						int B = clamp_uint8(((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024);
						*d = gray(R, G, B);
					}
				}
			}
			return newimage;
		}
		if (sformat == Format::YUYV8) {
			Image newimage(w, h, dformat);
			if (cuda) {
				global->cuda_plugin->convert_yuyv_to_gray(w, h, this->bits(), newimage.bits());
			} else {
				for (int y = 0; y < h; y++) {
					uint8_t const *s = scanLine(y);
					uint8_t *d = newimage.scanLine(y);
					uint8_t Y, U, V;
					U = V = 0;
					int w2 = w / 2;
					for (int x = 0; x < w2; x++) {
						int R, G, B;
						U = s[1];
						V = s[3];
						Y = s[0];
						R = clamp_uint8(((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024);
						G = clamp_uint8(((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024);
						B = clamp_uint8(((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024);
						*d++ = gray(R, G, B);
						Y = s[2];
						R = clamp_uint8(((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024);
						G = clamp_uint8(((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024);
						B = clamp_uint8(((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024);
						*d++ = gray(R, G, B);
						s += 4;
					}
					if (w & 1) {
						U = s[1];
						Y = s[0];
						int R = clamp_uint8(((Y - 16) * 1192 +                    (V - 128) * 1634) / 1024);
						int G = clamp_uint8(((Y - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024);
						int B = clamp_uint8(((Y - 16) * 1192 + (U - 128) * 2065                   ) / 1024);
						*d = gray(R, G, B);
					}
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
					d[1] = (*s++) * 225 / 256 + 16;
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
					d[0] = (*s++) * 225 / 256 + 16;
					d[1] = 128;
					d += 2;
				}
			}
			return newimage;
		}
	}

	return {};
}
