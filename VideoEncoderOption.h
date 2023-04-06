#ifndef VIDEOENCODEROPTION_H
#define VIDEOENCODEROPTION_H

#include "Rational.h"
#include <functional>

namespace VideoEncoderInternal {
class AudioFrame;
class VideoFrame;
}

namespace VideoEncoderOption {
enum class Format {
	MPEG4,
	H264_NVENC,
	HEVC_NVENC,
	LIBSVTAV1,
};
struct AudioOption {
	bool active = false;
	bool drop_if_overflow = true;
	int sample_rate = 48000;
	int channels = 2;
};
struct VideoOption {
	bool active = false;
	bool drop_if_overflow = true;
	int src_w = 1920;
	int src_h = 1080;
	int dst_w = 1920;
	int dst_h = 1080;
	Rational fps = {30, 1};
};
}


#endif // VIDEOENCODEROPTION_H
