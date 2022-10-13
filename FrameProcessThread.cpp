
#include "FrameProcessThread.h"
#include "ImageUtil.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include <QDebug>
#include "Deinterlace.h"

extern "C" {
#include <libswscale/swscale.h>
}

struct FrameProcessThread::Private {
	std::mutex mutex;
	std::condition_variable cond;
	bool interrupted = false;
	std::vector<std::thread> thread;
	std::deque<std::shared_ptr<CaptureFrame>> requested_frames;
	QSize scaled_size;
	Deinterlace di;
	bool deinterlace_enabled = true;
};

FrameProcessThread::FrameProcessThread()
	: m(new Private)
{
}

FrameProcessThread::~FrameProcessThread()
{
	stop();
	delete m;
}

static QImage scale(Image const &srcimg, int w, int h, QImage::Format f)
{
	AVPixelFormat sf = AV_PIX_FMT_NONE;
	AVPixelFormat df = AV_PIX_FMT_NONE;

	switch (srcimg.format()) {
	case Image::Format::RGB8:
		sf = AV_PIX_FMT_RGB24;
		break;
	case Image::Format::UYVY8:
		sf = AV_PIX_FMT_UYVY422;
		break;
	case Image::Format::YUYV8:
		sf = AV_PIX_FMT_YUYV422;
		break;
	case Image::Format::UINT8:
		sf = AV_PIX_FMT_GRAY8;
		break;
	default:
		return {};
	}

	switch (f) {
	case QImage::Format_RGB888:
		df = AV_PIX_FMT_RGB24;
		break;
	case QImage::Format_Grayscale8:
		df = AV_PIX_FMT_GRAY8;
		break;
	default:
		return {};
	}

	QImage newimg(w, h, f);

	SwsContext *c = sws_getContext(srcimg.width(), srcimg.height(), sf, w, h, df, SWS_POINT, nullptr, nullptr, nullptr);
	uint8_t const *srcdata[] = { srcimg.bits() };
	uint8_t *dstdata[] = { newimg.bits() };
	int srclines[] = { srcimg.bytesPerLine() };
	int dstlines[] = { newimg.bytesPerLine() };
	sws_scale(c, srcdata, srclines, 0, srcimg.height(), dstdata, dstlines);
	sws_freeContext(c);

	return newimg;
}

void FrameProcessThread::run()
{
	while (1) {
		std::shared_ptr<CaptureFrame> frame;
		{
			std::unique_lock lock(m->mutex);
			if (m->interrupted) break;
			for (size_t i = 0; i < m->requested_frames.size(); i++) {
				if (m->requested_frames[i]->d->state == CaptureFrame::Idle) {
					frame = m->requested_frames[i];
					frame->d->state = CaptureFrame::Busy;
					break;
				}
			}
			if (!frame) {
				m->cond.wait(lock);
			}
		}
		if (frame) {
			if (m->deinterlace_enabled) {
				frame->d->image = m->di.deinterlace(frame->d->image);
			}

			// 画面表示用画像
#if 0
			frame->d->image_for_view = ImageUtil::qimage(frame->d->image).scaled(m->scaled_size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
#else
			frame->d->image_for_view = scale(frame->d->image, m->scaled_size.width(), m->scaled_size.height(), QImage::Format_RGB888);
#endif

			frame->d->state = CaptureFrame::Ready;
		}

		std::deque<std::shared_ptr<CaptureFrame>> results;
		{
			std::unique_lock lock(m->mutex);
			while (!m->requested_frames.empty()) {
				if (m->requested_frames.front()->d->state != CaptureFrame::Ready) break;
				frame = m->requested_frames.front();
				m->requested_frames.pop_front();
				results.push_back(frame);
			}
		}
		while (!results.empty()) {
			frame = results.front();
			results.pop_front();
			emit ready(*frame);
		}
	}
}

void FrameProcessThread::start()
{
	stop();
	m->thread.resize(4);
	for (size_t i = 0; i < m->thread.size(); i++) {
		m->thread[i] = std::thread([&](){
			run();
		});
	}
}

void FrameProcessThread::stop()
{
	{
		std::lock_guard lock(m->mutex);
		m->interrupted = true;
		m->requested_frames = {};
	}
	m->cond.notify_all();
	for (size_t i = 0; i < m->thread.size(); i++) {
		if (m->thread[i].joinable()) {
			m->thread[i].join();
		}
	}
	m->thread.clear();
	m->interrupted = false;
}

void FrameProcessThread::request(CaptureFrame const &image, const QSize &size)
{
	if (image && size.width() > 0 && size.height() > 0) {
		std::lock_guard lock(m->mutex);
		while (m->requested_frames.size() > 4) {
			m->requested_frames.pop_back(); // drop
		}
		m->requested_frames.push_back(std::make_shared<CaptureFrame>(image));
		m->scaled_size = size;
		m->cond.notify_all();
	}
}

void FrameProcessThread::enableDeinterlace(bool enable)
{
	m->deinterlace_enabled = enable;
}
