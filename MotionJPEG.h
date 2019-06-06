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
	~MotionJPEG();
	void start(QString const &filepath, int width, int height, bool audio);
	void stop();
	void writeVideoFrame(const QByteArray &jpeg);
	void writeAudio(const QByteArray &wave);
	bool putVideoFrame(const QImage &img);
	bool putAudioSamples(const QByteArray &wav);
	bool create();
	void close();
};

#endif // MOTIONJPEG_H
