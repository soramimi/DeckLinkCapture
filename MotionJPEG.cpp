#include "MotionJPEG.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QMutex>
#include <deque>

struct MotionJPEG::Private {
	QMutex mutex;
	bool recording = false;
	QString filepath;
	int width = 1200;
	int height = 675;
	double fps = 29.97;
	std::deque<QImage> frames;
	QFile file;
	uint64_t pos_AVI = 0;
	uint64_t pos_dwTotalFrames = 0;
	uint64_t pos_dwLength = 0;
	uint64_t pos_movi = 0;
	int total_frames = 0;
};


MotionJPEG::MotionJPEG()
	: m(new Private)
{
}

MotionJPEG::~MotionJPEG()
{
	stop();
	delete m;
}

void MotionJPEG::write(char c)
{
	m->file.putChar(c);
}

void MotionJPEG::write(const char *p, int n)
{
	m->file.write(p, n);
}

void MotionJPEG::write16LE(uint16_t v)
{
	uint8_t tmp[2];
	tmp[0] = (uint8_t)v;
	tmp[1] = (uint8_t)(v >> 8);
	m->file.write((char const *)tmp, 2);
}

void MotionJPEG::write32LE(uint32_t v)
{
	uint8_t tmp[4];
	tmp[0] = (uint8_t)v;
	tmp[1] = (uint8_t)(v >> 8);
	tmp[2] = (uint8_t)(v >> 16);
	tmp[3] = (uint8_t)(v >> 24);
	m->file.write((char const *)tmp, 4);
}

void MotionJPEG::writeLength(QIODevice *file, uint64_t p)
{
	uint64_t q = file->pos();
	file->seek(p - 4);
	write32LE(q - p);
	file->seek(q);
}

QByteArray MotionJPEG::nextFrame()
{
	QImage img;
	{
		QMutexLocker lock(&m->mutex);
		if (!m->frames.empty()) {
			img = m->frames.front();
			m->frames.pop_front();
		}
	}
	QByteArray ba;
	if (img.width() > 0 && img.height() > 0) {
		img = img.scaled(m->width, m->height);
		QBuffer buf;
		buf.open(QBuffer::WriteOnly);
		img.save(&buf, "jpeg");
		buf.close();
		ba = buf.data();
	}
	return ba;
}

bool MotionJPEG::create()
{
	m->file.setFileName(m->filepath);
	if (!m->file.open(QFile::WriteOnly)) {
		return false;
	}

	const int scale = 100;

	write("RIFF", 4);
	{
		write32LE(0);
		m->pos_AVI = m->file.pos();
		write("AVI ", 4);
		write("LIST", 4);
		{
			write32LE(0);
			uint64_t p0 = m->file.pos();
			write("hdrl", 4);
			write("avih", 4);
			{
				write32LE(0);
				uint64_t p1 = m->file.pos();
				write32LE(0); // dwMicroSecPerFrame
				write32LE(0); // dwMaxBytesPerSec
				write32LE(0); // dwPaddingGranularity
				write32LE(0); // dwFlags
				m->pos_dwTotalFrames = m->file.pos();
				write32LE(m->total_frames); // dwTotalFrames
				write32LE(0); // dwInitialFrames
				write32LE(1); // dwStreams
				write32LE(0); // dwSuggestedBufferSize
				write32LE(m->width); // dwWidth
				write32LE(m->height); // dwHeight
				write32LE(0); // reserved
				write32LE(0); // reserved
				write32LE(0); // reserved
				write32LE(0); // reserved
				writeLength(&m->file, p1);
			}

			write("LIST", 4);
			{
				write32LE(0);
				uint64_t p1 = m->file.pos();
				write("strl", 4);

				write("strh", 4);
				write32LE(0);
				uint64_t p2 = m->file.pos();
				write("vids", 4);
				write("mpjg", 4);
				write32LE(0); // dwFlags
				write16LE(0); // wPriority
				write16LE(0); // wLanguage
				write32LE(0); // dwInitialFrames
				write32LE(scale); // dwScale
				write32LE(static_cast<uint32_t>(m->fps * scale)); // dwRate
				write32LE(0); // dwStart
				m->pos_dwLength = m->file.pos();
				write32LE(m->total_frames); // dwLength
				write32LE(0); // dwSuggestedBufferSize
				write32LE(0); // dwQuality
				write32LE(0); // dwSampleSize
				write16LE(0); // rcFrame.left
				write16LE(0); // rcFrame.top
				write16LE(m->width); // rcFrame.right
				write16LE(m->height); // rcFrame.bottom
				writeLength(&m->file, p2);

				write("strf", 4);
				write32LE(0);
				uint64_t p3 = m->file.pos();
				write32LE(0x28); // biSize
				write32LE(m->width); // biWidth
				write32LE(m->height); // biHeight
				write16LE(1); // biPlanes
				write16LE(24); // biBitCount
				write("MJPG", 4); // biCompression
				write32LE(m->width * m->height * 3); // biSizeImage
				write32LE(0); // biXPelsPerMeter
				write32LE(0); // biYPelsPerMete;
				write32LE(0); // biClrUsed
				write32LE(0); // biClrImportant
				writeLength(&m->file, p3);

				writeLength(&m->file, p1);
			}

			writeLength(&m->file, p0);
		}

		write("JUNK", 4);
		write32LE(0);

		write("LIST", 4);
		{
			write32LE(0);
			m->pos_movi = m->file.pos();
			write("movi", 4);
		}
	}
	return true;
}

void MotionJPEG::close()
{
	if (m->file.isOpen()) {
		writeLength(&m->file, m->pos_movi);
		writeLength(&m->file, m->pos_AVI);
		m->file.seek(m->pos_dwTotalFrames);
		write32LE(m->total_frames); // dwTotalFrames
		m->file.seek(m->pos_dwLength);
		write32LE(m->total_frames); // dwLength
		m->file.close();
	}
}

void MotionJPEG::writeFrame(QByteArray const &jpeg)
{
	uint32_t len = jpeg.size();
	bool pad = len & 1;
	if (pad) len++;

	write("00dc", 4);
	write32LE(len);
	m->file.write(jpeg);
	if (pad) m->file.putChar(0);

	m->total_frames++;
}

void MotionJPEG::run()
{
	create();

	if (m->file.isOpen()) {
		while (1) {
			if (isInterruptionRequested()) {
				break;
			}
			QByteArray jpeg = nextFrame();
			if (jpeg.isEmpty()) {
				QThread::yieldCurrentThread();
				continue;
			}
			writeFrame(jpeg);
		}
	}

	close();
	m->recording = false;
}

void MotionJPEG::start(const QString &filepath, int width, int height)
{
	m->filepath = filepath;
	m->width = width;
	m->height = height;
	m->recording = true;
	QThread::start();
}

void MotionJPEG::stop()
{
	if (m->recording) {
		requestInterruption();
		if (!wait(1000)) {
			terminate();
		}
	}
}

bool MotionJPEG::putFrame(const QImage &img)
{
	if (m->recording) {
		QMutexLocker lock(&m->mutex);
		if (m->frames.size() < 100) {
			m->frames.push_back(img);
			return true;
		}
	}
	return false;
}

