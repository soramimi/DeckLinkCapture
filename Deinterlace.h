
// Deinterlace filter
// Copyright (C) 2019 S.Fuchita (@soramimi_jp)
// MIT License

#ifndef DEINTERLACE_H
#define DEINTERLACE_H

#include <stdint.h>
#include <vector>
#include <memory>
#include <QImage>

class Deinterlace {
private:
	struct Task {
		int y = 0;
		int h = 0;
		int w = 0;
		uint8_t *dst = nullptr;
		uint8_t const *src0 = nullptr;
		uint8_t const *src1 = nullptr;
	};
public:
	std::pair<QImage, QImage> filter(QImage image);
};

#endif // DEINTERLACE_H
