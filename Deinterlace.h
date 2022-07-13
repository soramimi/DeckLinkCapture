#ifndef DEINTERLACE_H
#define DEINTERLACE_H

#include "Image.h"
#include <mutex>
#include <condition_variable>

class Deinterlace {
private:
	std::mutex mutex_;
	std::condition_variable cond_;
	int image_index_ = 0;
	int image_count_ = 0;
	static const int NBUF = 8;
	Image image_buffer_[NBUF];
	template <typename T> Image process(const Image &input);
public:
	Image deinterlace(const Image &input);
};

#endif
