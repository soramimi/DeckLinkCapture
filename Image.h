#ifndef IMAGE_H
#define IMAGE_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <utility>

class Image {
public:
	enum class Format {
		None,
		RGB8,
		UYVY8,
		YUYV8,
		UINT8,
		UINT16,
		UINT32,
	};
	static int bytesPerPixel(Format f)
	{
		switch (f) {
		case Format::UINT8:
			return 1;
		case Format::RGB8:
			return 3;
		case Format::UYVY8:
		case Format::YUYV8:
		case Format::UINT16:
			return 2;
		case Format::UINT32:
			return 4;
		}
		return 0;
	}
private:
	struct Core {
		unsigned int ref = 0;
		int width = 0;
		int height = 0;
		enum Format format = Format::RGB8;
		char *data() const
		{
			return (char *)this + sizeof(Core);
		}
//		char data[0];
	};
	Core *core_ = nullptr;
	void assign(Core *p)
	{
		if (p) {
			p->ref++;
		}
		if (core_) {
			if (core_->ref > 1) {
				core_->ref--;
			} else {
				core_->~Core();
				free(core_);
			}
		}
		core_ = p;
	}
	void copy_on_write()
	{
		if (core_ && core_->ref > 1) {
			Image img = copy();
			assign(img.core_);
		}
	}
public:
	Image()
	{
	}
	Image(int w, int h, Format format)
	{
		create(w, h, format);
	}
	~Image()
	{
		clear();
	}
	Image(Image const &t)
	{
		assign(t.core_);
	}
	void operator = (Image const &t)
	{
		assign(t.core_);
	}
	void clear()
	{
		assign(nullptr);
	}
	bool isNull() const
	{
		return !core_;
	}
	operator bool () const
	{
		return !isNull();
	}
	void create(int w, int h, Format format)
	{
		size_t datalen = bytesPerPixel(format) * w * h;
		Core *p = (Core *)malloc(sizeof(Core) + datalen);
		*p = {};
		p->width = w;
		p->height = h;
		p->format = format;
		assign(p);
	}
	int width() const
	{
		return core_ ? core_->width : 0;
	}
	int height() const
	{
		return core_ ? core_->height : 0;
	}
	Format format() const
	{
		return core_ ? core_->format : Format::None;
	}
	int bytesPerPixel() const
	{
		return bytesPerPixel(format());
	}
	int bytesPerLine() const
	{
		return bytesPerPixel() * width();
	}
	uint8_t const *scanLine(int y) const
	{
		return core_ ? ((uint8_t const *)core_->data() + bytesPerLine() * y) : nullptr;
	}
	uint8_t *scanLine(int y)
	{
		copy_on_write();
		return core_ ? ((uint8_t *)core_->data() + bytesPerLine() * y) : nullptr;
	}
	uint8_t const *bits() const
	{
		return core_ ? (uint8_t const *)core_->data() : nullptr;
	}
	uint8_t *bits()
	{
		copy_on_write();
		return core_ ? (uint8_t *)core_->data() : nullptr;
	}
	Image copy() const
	{
		Image newimg;
		if (core_) {
			int w = width();
			int h = height();
			Format f = format();
			newimg.create(w, h, f);
			memcpy(newimg.core_->data(), core_->data(), bytesPerPixel(f) * w * h);
		}
		return newimg;
	}
	Image convertToFormat(Image::Format dformat) const;

	void swap(Image &r)
	{
		std::swap(core_, r.core_);
	}
};

namespace std {
template <> inline void swap<Image>(Image &l, Image &r)
{
	l.swap(r);
}
}

#endif // IMAGE_H
