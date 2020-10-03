#ifndef IMAGEUTIL_H
#define IMAGEUTIL_H

#include <QImage>
#include "Image.h"

class ImageUtil {
public:
	static QImage qimage(Image const &image);
	static Image image(QImage const &image, Image::Format format);
};

#endif // IMAGEUTIL_H
