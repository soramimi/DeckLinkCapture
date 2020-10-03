#ifndef IMAGE_H
#define IMAGE_H

#include <stdlib.h>
#include <string.h>

class Image {
public:
	enum class Format {
		None,
		RGB8,
		UYUV8,
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
		case Format::UYUV8:
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
		char data[0];
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
			assign(copy().core_);
		}
	}
public:
	Image()
	{
	}
	Image(int w, int h, Format format)
	{
		size_t datalen = bytesPerPixel(format) * w * h;
		Core *p = (Core *)malloc(sizeof(Core) + datalen);
		p->width = w;
		p->height = h;
		p->format = format;
		assign(p);
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
	u_int8_t const *scanLine(int y) const
	{
		return core_ ? ((u_int8_t const *)core_->data + bytesPerLine() * y) : nullptr;
	}
	u_int8_t *scanLine(int y)
	{
		copy_on_write();
		return core_ ? ((u_int8_t *)core_->data + bytesPerLine() * y) : nullptr;
	}
	u_int8_t const *bits() const
	{
		return core_ ? (u_int8_t const *)core_->data : nullptr;
	}
	u_int8_t *bits()
	{
		copy_on_write();
		return core_ ? (u_int8_t *)core_->data : nullptr;
	}
	Image copy() const
	{
		Image newimg;
		if (core_) {
			size_t datalen = bytesPerLine() * height();
			Core *p = (Core *)malloc(sizeof(Core) + datalen);
			*p = *core_;
			p->ref = 0;
			memcpy(p->data, core_->data, datalen);
			newimg.assign(p);
		}
		return newimg;
	}
	Image convertToFormat(Image::Format dformat) const;
};

#endif // IMAGE_H
