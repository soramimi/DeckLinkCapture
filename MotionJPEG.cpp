#include "MotionJPEG.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QMutex>
#include <deque>

struct MotionJPEG::Private {
	QMutex mutex;
	QString filepath;
	QFile file;
	bool thread_recording = false;
	MotionJPEG::VideoOption vopt;
	MotionJPEG::AudioOption aopt;
	std::deque<QImage> video_frames;
	std::deque<QByteArray> audio_frames;
	uint64_t pos_riff = 0;
	uint64_t pos_dwTotalFrames = 0;
	uint64_t pos_vids_strh_dwLength = 0;
	uint64_t pos_movi = 0;
	uint32_t total_frames = 0;
};


MotionJPEG::MotionJPEG()
	: m(new Private)
{
}

MotionJPEG::~MotionJPEG()
{
	thread_stop();
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

QByteArray MotionJPEG::nextVideoFrame()
{
	QImage img;
	{
		QMutexLocker lock(&m->mutex);
		if (!m->video_frames.empty()) {
			img = m->video_frames.front();
			m->video_frames.pop_front();
		}
	}
	QByteArray ba;
	if (img.width() > 0 && img.height() > 0) {
		img = img.scaled(m->vopt.width, m->vopt.height);
		QBuffer buf;
		buf.open(QBuffer::WriteOnly);
		img.save(&buf, "jpeg");
		buf.close();
		ba = buf.data();
	}
	return ba;
}

QByteArray MotionJPEG::nextAudioSamples()
{
	QByteArray ba;
	{
		QMutexLocker lock(&m->mutex);
		if (!m->audio_frames.empty()) {
			ba = m->audio_frames.front();
			m->audio_frames.pop_front();
		}
	}
	return ba;
}

bool MotionJPEG::config(const QString &filepath, const VideoOption &vopt, const AudioOption &aopt)
{
	m->filepath = filepath;
	m->vopt = vopt;
	m->aopt = aopt;
}

bool MotionJPEG::create()
{
	m->file.setFileName(m->filepath);
	if (!m->file.open(QFile::WriteOnly)) {
		return false;
	}

	const int scale = 100;

	write("RIFF", 4);
	write32LE(0);
	m->pos_riff = m->file.pos();
	{
		write("AVI ", 4);

		write("LIST", 4);
		write32LE(0);
		uint64_t pos_avi_list = m->file.pos();
		{
			write("hdrl", 4);

			write("avih", 4);
			write32LE(0);
			uint64_t pos_avih = m->file.pos();
			{
				write32LE(0); // dwMicroSecPerFrame
				write32LE(0); // dwMaxBytesPerSec
				write32LE(0); // dwPaddingGranularity
				write32LE(0); // dwFlags
				m->pos_dwTotalFrames = m->file.pos();
				write32LE(m->total_frames); // dwTotalFrames
				write32LE(0); // dwInitialFrames
				write32LE(1); // dwStreams
				write32LE(0); // dwSuggestedBufferSize
				write32LE(m->vopt.width); // dwWidth
				write32LE(m->vopt.height); // dwHeight
				write32LE(0); // reserved
				write32LE(0); // reserved
				write32LE(0); // reserved
				write32LE(0); // reserved
			}
			writeLength(&m->file, pos_avih);

			write("LIST", 4);
			write32LE(0);
			uint64_t pos_vids_list = m->file.pos();
			{
				write("strl", 4);

				write("strh", 4);
				write32LE(0);
				uint64_t pos_strh = m->file.pos();
				write("vids", 4);
				write("mpjg", 4); // fccHandler
				write32LE(0); // dwFlags
				write16LE(0); // wPriority
				write16LE(0); // wLanguage
				write32LE(0); // dwInitialFrames
				write32LE(scale); // dwScale
				write32LE(static_cast<uint32_t>(m->vopt.fps * scale)); // dwRate
				write32LE(0); // dwStart
				m->pos_vids_strh_dwLength = m->file.pos();
				write32LE(m->total_frames); // dwLength
				write32LE(0); // dwSuggestedBufferSize
				write32LE(0); // dwQuality
				write32LE(0); // dwSampleSize
				write16LE(0); // rcFrame.left
				write16LE(0); // rcFrame.top
				write16LE(m->vopt.width); // rcFrame.right
				write16LE(m->vopt.height); // rcFrame.bottom
				writeLength(&m->file, pos_strh);

				write("strf", 4);
				write32LE(0);
				uint64_t pos_strf = m->file.pos();
				write32LE(0x28); // biSize
				write32LE(m->vopt.width); // biWidth
				write32LE(m->vopt.height); // biHeight
				write16LE(1); // biPlanes
				write16LE(24); // biBitCount
				write("MJPG", 4); // biCompression
				write32LE(m->vopt.width * m->vopt.height * 3); // biSizeImage
				write32LE(0); // biXPelsPerMeter
				write32LE(0); // biYPelsPerMete;
				write32LE(0); // biClrUsed
				write32LE(0); // biClrImportant
				writeLength(&m->file, pos_strf);
			}
			writeLength(&m->file, pos_vids_list);

			if (m->aopt.channels != 0) {

				write("LIST", 4);
				write32LE(0);
				uint64_t pos_auds_list = m->file.pos();
				{
					write("strl", 4);

					write("strh", 4);
					write32LE(0);
					uint64_t pos_auds_strh = m->file.pos();
					write("auds", 4);
					write32LE(0); // fccHandler
					write32LE(0); // dwFlags
					write16LE(0); // wPriority
					write16LE(0); // wLanguage
					write32LE(0); // dwInitialFrames
					write32LE(1); // dwScale
					write32LE(m->aopt.frequency); // dwRate
					write32LE(0); // dwStart
					write32LE(0); // dwLength
					write32LE(0); // dwSuggestedBufferSize
					write32LE(0); // dwQuality
					write32LE(0); // dwSampleSize
					write16LE(0); // rcFrame.left
					write16LE(0); // rcFrame.top
					write16LE(0); // rcFrame.right
					write16LE(0); // rcFrame.bottom
					writeLength(&m->file, pos_auds_strh);

#define WAVE_FORMAT_PCM 1
					write("strf", 4);
					write32LE(0);
					uint64_t pos_auds_strf = m->file.pos();
					write16LE(WAVE_FORMAT_PCM); // wFormatTag
					write16LE(m->aopt.channels); // nChannels
					write32LE(m->aopt.frequency); // nSamplePerSec
					write32LE(m->aopt.frequency * m->aopt.channels * 2); // nAvgBytesPerSecbiPlanes
					write16LE(m->aopt.channels * 2); // nBlockAlign
					write16LE(m->aopt.channels * 8); // wBitsPerSample
					write16LE(0); // cbSize
					writeLength(&m->file, pos_auds_strf);
				}
				writeLength(&m->file, pos_auds_list);
			}

			writeLength(&m->file, pos_avi_list);
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
		writeLength(&m->file, m->pos_riff);
		m->file.seek(m->pos_dwTotalFrames);
		write32LE(m->total_frames); // dwTotalFrames
		m->file.seek(m->pos_vids_strh_dwLength);
		write32LE(m->total_frames); // dwLength
		m->file.close();
	}
}

void MotionJPEG::writeVideoFrame(QByteArray const &jpg)
{
	uint32_t len = jpg.size();
	bool pad = len & 1;
	if (pad) len++;

	write("00dc", 4);
	write32LE(len);
	m->file.write(jpg);
	if (pad) m->file.putChar(0);

	m->total_frames++;
}

void MotionJPEG::writeAudioFrame(QByteArray const &pcm)
{
	uint32_t len = pcm.size();
	bool pad = len & 1;
	if (pad) len++;

	write("01wb", 4);
	write32LE(len);
	m->file.write(pcm);
	if (pad) m->file.putChar(0);
}

void MotionJPEG::run()
{
	create();

	if (m->file.isOpen()) {
		while (1) {
			if (isInterruptionRequested()) {
				break;
			}
			bool empty = true;
			{
				QByteArray jpeg = nextVideoFrame();
				if (!jpeg.isEmpty()) {
					writeVideoFrame(jpeg);
					empty = false;
				}
			}
			{
				QByteArray wave = nextAudioSamples();
				if (!wave.isEmpty()) {
					writeAudioFrame(wave);
					empty = false;
				}
			}
			if (empty) {
				QThread::yieldCurrentThread();
			}
		}
	}

	close();
	m->thread_recording = false;
}

void MotionJPEG::thread_start(const QString &filepath, const VideoOption &vopt, AudioOption const &audio)
{
	config(filepath, vopt, audio);
	m->thread_recording = true;
	QThread::start();
}

void MotionJPEG::thread_stop()
{
	if (m->thread_recording) {
		requestInterruption();
		if (!wait(1000)) {
			terminate();
		}
	}
}

bool MotionJPEG::putVideoFrame(const QImage &img)
{
	if (m->thread_recording) {
		QMutexLocker lock(&m->mutex);
		if (m->video_frames.size() < 100) {
			m->video_frames.push_back(img);
			return true;
		}
	}
	return false;
}

bool MotionJPEG::putAudioSamples(const QByteArray &wav)
{
	if (m->thread_recording) {
		QMutexLocker lock(&m->mutex);
		if (m->audio_frames.size() < 100) {
			m->audio_frames.push_back(wav);
			return true;
		}
	}
	return false;
}

