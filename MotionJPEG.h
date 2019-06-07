#ifndef MOTIONJPEG_H
#define MOTIONJPEG_H

#include <QThread>

class QIODevice;

class MotionJPEG : public QThread {
private:
	struct Private;
	Private *m;
private:

	void write(char c);
	void write(char const *p, int n);
	void write16LE(uint16_t v);
	void write32LE(uint32_t v);
	void writeLength(QIODevice *file, uint64_t p);

	QByteArray nextVideoFrame();
	QByteArray nextAudioSamples();

	void run();
public:
	MotionJPEG();
	virtual ~MotionJPEG();

	struct VideoOption {
		int width = 960;
		int height = 540;
		double fps = 29.97;
	};

	struct AudioOption {
		int channels = 2;
		int frequency = 48000;

		static AudioOption None()
		{
			AudioOption o;
			o.channels = 0;
			return o;
		}
	};

	bool config(const QString &filepath, VideoOption const &vopt, AudioOption const &aopt);
	bool create();
	void close();
	void thread_start(QString const &filepath, VideoOption const &vopt, AudioOption const &aopt);
	void thread_stop();
	void writeVideoFrame(const QByteArray &jpg);
	void writeAudioFrame(const QByteArray &pcm);
	bool putVideoFrame(const QImage &img);
	bool putAudioSamples(const QByteArray &wav);
};

#endif // MOTIONJPEG_H
