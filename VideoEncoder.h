#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include "Image.h"
#include "Rational.h"
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <QByteArray>

struct AVCodecContext;
struct AVFormatContext;
struct AVCodec;
struct AVStream;

class CaptureFrame;

class VideoEncoder {
public:
	class MyPicture;
	class AudioFrame {
	public:
		QByteArray samples;
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
private:
	struct Private;
	Private *m;
public:
	struct AudioOption {
		int sample_rate = 48000;
	};
	struct VideoOption {
		int src_w = 1920;
		int src_h = 1080;
		int dst_w = 1920;
		int dst_h = 1080;
		Rational fps;
	};
private:
	bool is_interruption_requested() const;

	bool get_audio_frame(int16_t *samples, int frame_size, int nb_channels);
	bool get_video_frame(MyPicture *pict);
	bool open_audio(AVCodecContext *cc, AVCodec const *codec, AVStream *st, const AudioOption &opt);
	bool next_audio_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush);
	void close_audio();
	bool open_video(AVCodecContext *cc, const AVCodec *codec, AVStream *st, const VideoOption &opt);
	bool next_video_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush);
	void close_video();
	bool is_recording() const;
	void run();
	bool put_video_frame(const VideoFrame &img);
	bool put_audio_frame(const AudioFrame &pcm);
public:
	VideoEncoder();
	virtual ~VideoEncoder();
	void request_interruption();
	void open(std::string const &filepath, VideoOption const &vopt, AudioOption const &aopt);
	void close();
	void put_frame(CaptureFrame const &frame);
};

#endif // VIDEOENCODER_H
