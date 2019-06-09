#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <QThread>
#include <stdint.h>

struct AVFormatContext;
struct AVCodec;
struct AVStream;
struct AVPicture;

class QImage;
class QByteArray;

class VideoEncoder : public QThread {
private:
	struct Private;
	Private *m;
protected:
	bool get_audio_frame(int16_t *samples, int frame_size, int nb_channels);
	bool get_video_frame(AVPicture *pict, int frame_index, int width, int height);
	void open_audio(AVFormatContext *oc, AVCodec *codec, AVStream *st);
	void write_audio_frame(AVFormatContext *oc, AVStream *st, bool flush);
	void close_audio(AVFormatContext *oc, AVStream *st);
	void open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st);
	void write_video_frame(AVFormatContext *oc, AVStream *st, bool flush);
	void close_video(AVFormatContext *oc, AVStream *st);
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
	bool putVideoFrame(const QImage &img);
	bool putAudioFrame(const QByteArray &pcm);
};


#endif // VIDEOENCODER_H
