#ifndef FFMPEGVIDEOENCODER_H
#define FFMPEGVIDEOENCODER_H

#include "Image.h"
#include "VideoEncoderOption.h"
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <QByteArray>
#include <functional>

struct AVCodecContext;
struct AVFormatContext;
struct AVCodec;
struct AVStream;

class VideoFrameData;

namespace VideoEncoderInternal {
class AudioFrame {
public:
	QByteArray samples;
	operator bool () const
	{
		return !samples.isEmpty();
	}
};
class VideoFrame {
public:
	Image image;
	operator bool () const
	{
		return (bool)image;
	}
	int width() const
	{
		return image ? image.width() : 0;
	}
	int height() const
	{
		return image ? image.height() : 0;
	}
};
}

class FFmpegVideoEncoder {
public:
	class MyPicture;
private:
	struct Private;
	Private *m;

	bool is_interruption_requested() const;
	bool get_audio_frame(int16_t *samples, int frame_size, int nb_channels);
	bool get_video_frame(MyPicture *pict);
	bool open_audio(AVCodecContext *cc, AVCodec const *codec, AVStream *st, const VideoEncoderOption::AudioOption &opt);
	bool next_audio_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush);
	void close_audio();
	bool open_video(AVCodecContext *cc, const AVCodec *codec, AVStream *st, const VideoEncoderOption::VideoOption &opt);
	bool next_video_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush);
	void close_video();
	void run();
	bool put_video_frame(const VideoEncoderInternal::VideoFrame &img);
	bool put_audio_frame(const VideoEncoderInternal::AudioFrame &pcm);
	void default_get_video_frame(VideoEncoderInternal::VideoFrame *out);
	void default_get_audio_frame(int16_t *samples, int frame_size, int nb_channels);
	void request_interruption();
public:
	FFmpegVideoEncoder();
	virtual ~FFmpegVideoEncoder();
	bool create(std::string const &filepath, VideoEncoderOption::Format format, VideoEncoderOption::VideoOption const &vopt, VideoEncoderOption::AudioOption const &aopt);
	void close();
	bool is_recording() const;
	void put_frame(const VideoFrameData &frame);
	VideoEncoderOption::AudioOption const *audio_option() const;
	VideoEncoderOption::VideoOption const *video_option() const;
};

#endif // FFMPEGVIDEOENCODER_H
