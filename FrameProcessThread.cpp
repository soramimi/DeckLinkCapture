
#include "FrameProcessThread.h"
#include "ImageUtil.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include "Deinterlace.h"

struct FrameProcessThread::Private {
	std::mutex mutex;
	std::condition_variable cond;
	bool interrupted = false;
	std::vector<std::thread> thread;
	std::deque<std::shared_ptr<CaptureFrame>> requested_frames;
	QSize scaled_size;
	Deinterlace di;
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
			frame->d->image = m->di.deinterlace(frame->d->image);
			frame->d->image_for_view = ImageUtil::qimage(frame->d->image).scaled(m->scaled_size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
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
