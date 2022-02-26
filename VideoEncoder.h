#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <QThread>
#include <stdint.h>

struct AVCodecContext;
struct AVFormatContext;
struct AVCodec;
struct AVStream;

class QImage;
class QByteArray;

class MyPicture {
public:
	uint8_t *pointers[4] = {};
	int linesize[4] = {};
};

class VideoEncoder : public QThread {
private:
	struct Private;
	Private *m;
protected:
	bool get_audio_frame(int16_t *samples, int frame_size, int nb_channels);
	bool get_video_frame(MyPicture *pict, int frame_index, int width, int height);
	bool open_audio(AVCodecContext *cc, AVFormatContext *oc, AVCodec const *codec, AVStream *st);
	bool write_audio_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush);
	void close_audio(AVFormatContext *fc, AVStream *st);
	bool open_video(AVCodecContext *cc, AVFormatContext *fc, const AVCodec *codec, AVStream *st);
	bool write_video_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush);
	void close_video(AVFormatContext *fc, AVStream *st);
	bool is_recording() const;
	int save();
	void run();
public:
	struct VideoOption {
		double fps = 30;
	};
	struct AudioOption {
	};
	VideoEncoder();
	virtual ~VideoEncoder();
	void thread_start(QString const &filepath, VideoOption const &vopt, AudioOption const &aopt);
	void thread_stop();
	bool putVideoFrame(const Image &img);
	bool putAudioFrame(const QByteArray &pcm);
};


#endif // VIDEOENCODER_H
