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

	QByteArray nextFrame();

	void run();
public:
	MotionJPEG();
	~MotionJPEG();
	void start(QString const &filepath, int width, int height);
	void stop();
	bool putFrame(const QImage &img);
	bool create();
	void close();
	void writeFrame(const QByteArray &jpeg);
};

#endif // MOTIONJPEG_H
