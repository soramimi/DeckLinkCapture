#include "VideoEncoder.h"
#include "Image.h"
#include <assert.h>
#include <deque>
#include <math.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include "includeffmpeg.h"

namespace {

void print_averror(int ret)
{
	char tmp[1024];
	sprintf(tmp, "AVERROR(%d)", ret);
	std::string msg = tmp;
	if (ret < 0) {
		av_make_error_string(tmp, sizeof(tmp) - 1, ret);
		ret = -ret;
		char const *p = reinterpret_cast<char const *>(&ret);
		if (p) {
			msg += ": ";
			for (int i = 0; i < 4; i++) {
				if (p[i] > ' ') msg += p[i];
			}
		}
	}
	fprintf(stderr, "%s\n", msg.c_str());
}

int write_frame(AVCodecContext const *cc, AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	av_packet_rescale_ts(pkt, cc->time_base, st->time_base);
	pkt->stream_index = st->index;
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

} // namespace

class VideoEncoder::MyPicture {
public:
	uint8_t *pointers[4] = {};
	int linesize[4] = {};

	int alloc(int width, int height, int pixfmt)
	{
		return av_image_alloc(pointers, linesize, width, height, (AVPixelFormat)pixfmt, 1);
	}
	void free()
	{
		av_freep(pointers);
	}
};

struct VideoEncoder::Private {
	std::string filepath;
	VideoEncoder::VideoOption vopt;
	VideoEncoder::AudioOption aopt;

	bool audio_is_eof = false;
	bool video_is_eof = false;

	AVPixelFormat source_pixel_format = AV_PIX_FMT_YUYV422;

	AVFrame *audio_frame = nullptr;
	uint8_t **src_samples_data = 0;
	int src_samples_linesize = 0;
	int src_nb_samples = 0;
	int max_dst_nb_samples = 0;
	uint8_t **dst_samples_data = nullptr;
	int dst_samples_linesize = 0;
	int dst_samples_size = 0;
	int samples_count = 0;
	SwrContext *swr_ctx = nullptr;
	SwsContext *sws_ctx = nullptr;

	double audio_pts = 0;
	double video_pts = 0;

	AVCodecContext *video_codec_context = nullptr;
	AVCodecContext *audio_codec_context = nullptr;
	AVFrame *video_frame = nullptr;
	VideoEncoder::MyPicture src_picture;
	VideoEncoder::MyPicture dst_picture;
	int frame_count = 0;
	AVStream *audio_st = nullptr;
	AVStream *video_st = nullptr;

	std::mutex mutex;
	std::deque<VideoFrame> video_frames;
	std::deque<AudioFrame> audio_frames;

	std::thread thread;
	int ret = 0;
	bool interrupted = false;
};

VideoEncoder::VideoEncoder()
	: m(new Private)
{
}

VideoEncoder::~VideoEncoder()
{
	delete m;
}

void VideoEncoder::request_interruption()
{
	m->interrupted = true;
}

bool VideoEncoder::is_interruption_requested() const
{
	return m->interrupted;
}

bool VideoEncoder::get_audio_frame(int16_t *samples, int frame_size, int nb_channels)
{
	while (frame_size > 0) {
		std::lock_guard lock(m->mutex);
		if (m->audio_frames.empty()) break;
		AudioFrame &frame = m->audio_frames.front();
		int n = frame.samples->size() / nb_channels / sizeof(int16_t);
		n = std::min(n, frame_size);
		frame_size -= n;
		n *= nb_channels * sizeof(int16_t);
		memcpy(samples, frame.samples->data(), n);
		samples += n / sizeof(int16_t);
		if (frame.samples->size() < n + nb_channels * sizeof(int16_t)) {
			m->audio_frames.pop_front();
		} else {
			int l = frame.samples->size() - n;
			char *p = frame.samples->data();
			memmove(p, p + n, l);
			frame.samples->resize(l);
		}
	}
	return true;
}

bool VideoEncoder::open_audio(AVCodecContext *cc, AVCodec const *codec, AVStream *st)
{
	AVCodecParameters *cp = st->codecpar;

	m->audio_frame = av_frame_alloc();
	if (!m->audio_frame) {
		fprintf(stderr, "Could not allocate audio frame\n");
		return false;
	}
	m->audio_frame->format = st->codecpar->format;
	m->audio_frame->channels = st->codecpar->channels;

	/* open it */
	m->ret = avcodec_open2(cc, codec, nullptr);
	if (m->ret < 0) {
		return false;
	}
	cp->frame_size = cc->frame_size;
	m->src_nb_samples = cc->frame_size;
	m->ret = av_samples_alloc_array_and_samples(&m->src_samples_data, &m->src_samples_linesize, cp->channels, m->src_nb_samples, AV_SAMPLE_FMT_S16, 0);
	if (m->ret < 0) {
		fprintf(stderr, "Could not allocate source samples\n");
		return false;
	}
	/* compute the number of converted samples: buffering is avoided
	 * ensuring that the output buffer will contain at least all the
	 * converted input samples */
	m->max_dst_nb_samples = m->src_nb_samples;
	/* create resampler context */
	if (cp->format != AV_SAMPLE_FMT_S16) {
		m->swr_ctx = swr_alloc();
		if (!m->swr_ctx) {
			fprintf(stderr, "Could not allocate resampler context\n");
			return false;
		}
		/* set options */
		av_opt_set_int       (m->swr_ctx, "in_channel_count",   cp->channels,               0);
		av_opt_set_int       (m->swr_ctx, "in_sample_rate",     cp->sample_rate,            0);
		av_opt_set_sample_fmt(m->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16,          0);
		av_opt_set_int       (m->swr_ctx, "out_channel_count",  cp->channels,               0);
		av_opt_set_int       (m->swr_ctx, "out_sample_rate",    cp->sample_rate,            0);
		av_opt_set_sample_fmt(m->swr_ctx, "out_sample_fmt",     (AVSampleFormat)cp->format, 0);
		/* initialize the resampling context */
		if ((m->ret = swr_init(m->swr_ctx)) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			return false;
		}
		m->ret = av_samples_alloc_array_and_samples(&m->dst_samples_data, &m->dst_samples_linesize, cp->channels, m->max_dst_nb_samples, (AVSampleFormat)cp->format, 0);
		if (m->ret < 0) {
			fprintf(stderr, "Could not allocate destination samples\n");
			return false;
		}
	} else {
		m->dst_samples_data = m->src_samples_data;
	}
	m->dst_samples_size = av_samples_get_buffer_size(nullptr, cp->channels, m->max_dst_nb_samples, (AVSampleFormat)cp->format, 0);
	return true;
}

bool VideoEncoder::next_audio_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush)
{
	if (!st) return false;
	if (!m->audio_frame) return false;

	AVPacket pkt = {}; // data and size must be 0;
	int dst_nb_samples;
	av_init_packet(&pkt);
	AVCodecParameters *cp = st->codecpar;
	if (!flush) {
		if (!get_audio_frame((int16_t *)m->src_samples_data[0], m->src_nb_samples, cp->channels)) {
			return false;
		}
		/* convert samples from native format to destination codec format, using the resampler */
		if (m->swr_ctx) {
			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(m->swr_ctx, cp->sample_rate) + m->src_nb_samples, cp->sample_rate, cp->sample_rate, AV_ROUND_UP);
			if (dst_nb_samples > m->max_dst_nb_samples) {
				av_free(m->dst_samples_data[0]);
				m->ret = av_samples_alloc(m->dst_samples_data, &m->dst_samples_linesize, cp->channels, dst_nb_samples, (AVSampleFormat)cp->format, 0);
				if (m->ret < 0) {
					return false;
				}
				m->max_dst_nb_samples = dst_nb_samples;
				m->dst_samples_size = av_samples_get_buffer_size(nullptr, cp->channels, dst_nb_samples, (AVSampleFormat)cp->format, 0);
			}
			/* convert to destination format */
			m->ret = swr_convert(m->swr_ctx, m->dst_samples_data, dst_nb_samples, (const uint8_t **)m->src_samples_data, m->src_nb_samples);
			if (m->ret < 0) {
				fprintf(stderr, "Error while converting\n");
				return false;
			}
			dst_nb_samples = m->ret;
		} else {
			dst_nb_samples = m->src_nb_samples;
		}
		m->audio_frame->nb_samples = dst_nb_samples;
		AVRational rate = { 1, cp->sample_rate };
		m->audio_frame->pts = av_rescale_q(m->samples_count, rate, st->time_base);
		m->ret = avcodec_fill_audio_frame(m->audio_frame, cp->channels, (AVSampleFormat)cp->format, m->dst_samples_data[0], m->dst_samples_size, 0);
		m->samples_count += dst_nb_samples;
	}

	m->ret = avcodec_send_frame(cc, flush ? nullptr : m->audio_frame);
	if (m->ret < 0 && m->ret != AVERROR_EOF) {
		fprintf(stderr, "error during sending frame");
		return false;
	}

	while (avcodec_receive_packet(cc, &pkt) == 0) {
		m->ret = write_frame(cc, fc, &st->time_base, st, &pkt);
		if (m->ret != 0) {
			printf("av_write_frame failed\n");
		}
	}

	if (flush) {
		m->audio_is_eof = true;
	}

	m->audio_pts = (double)m->samples_count * st->time_base.den / st->time_base.num / cp->sample_rate;
	return true;
}

void VideoEncoder::close_audio()
{
	if (m->audio_codec_context) {
		avcodec_close(m->audio_codec_context);
		avcodec_free_context(&m->audio_codec_context);
	}
	if (m->dst_samples_data != m->src_samples_data) {
		av_free(m->dst_samples_data[0]);
		av_free(m->dst_samples_data);
	}
	av_free(m->src_samples_data[0]);
	av_free(m->src_samples_data);
	av_frame_free(&m->audio_frame);
}

bool VideoEncoder::get_video_frame(MyPicture *pict, int frame_index, int width, int height)
{
	VideoFrame img;
	{
		std::lock_guard lock(m->mutex);
		if (!m->video_frames.empty()) {
			std::swap(img, m->video_frames.front());
			m->video_frames.pop_front();
		}
	}
	if (img.width() != width || img.height() != height) return false;

	img.image = img.image.convertToFormat(Image::Format::YUYV8);
	uint8_t *p = pict->pointers[0];
	memcpy(p, img.image.bits(), img.image.bytesPerLine() * height);

	return true;
}

bool VideoEncoder::open_video(AVCodecContext *cc, AVCodec const *codec, AVStream *st)
{
	AVCodecParameters *c = st->codecpar;

	AVDictionary *codec_options = nullptr;

	m->ret = avcodec_open2(cc, codec, &codec_options);
	if (m->ret < 0) {
		return false;
	}

	m->video_frame = av_frame_alloc();
	if (!m->video_frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		return false;
	}
	m->video_frame->format = c->format;
	m->video_frame->width = c->width;
	m->video_frame->height = c->height;

	m->ret = m->dst_picture.alloc(c->width, c->height, c->format);
	if (m->ret < 0) {
		return false;
	}

	m->ret = m->src_picture.alloc(c->width, c->height, m->source_pixel_format);
	if (m->ret < 0) {
		return false;
	}

	for (int i = 0; i < 4; i++) {
		m->video_frame->data[i] = m->dst_picture.pointers[i];
		m->video_frame->linesize[i] = m->dst_picture.linesize[i];
	}

	return true;
}

bool VideoEncoder::next_video_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush)
{
	if (!st) return false;
	if (!m->video_frame) return false;

	AVCodecParameters *c = st->codecpar;
	if (!flush) {
		if (!m->sws_ctx) {
			m->sws_ctx = sws_getContext(c->width, c->height, m->source_pixel_format, c->width, c->height, (AVPixelFormat)c->format, SWS_BILINEAR, nullptr, nullptr, nullptr);
			if (!m->sws_ctx) {
				fprintf(stderr, "Could not initialize the conversion context\n");
				exit(1);
			}
		}
		if (!get_video_frame(&m->src_picture, m->frame_count, c->width, c->height)) {
			return false;
		}
		sws_scale(m->sws_ctx, (const uint8_t * const *)m->src_picture.pointers, m->src_picture.linesize, 0, c->height, m->dst_picture.pointers, m->dst_picture.linesize);
	}
	{
		AVPacket pkt = {};
		av_init_packet(&pkt);

		m->video_frame->pts = m->frame_count;

		m->ret = avcodec_send_frame(cc, flush ? nullptr : m->video_frame);
		if (m->ret < 0 && m->ret != AVERROR_EOF) {
			fprintf(stderr, "error during sending frame");
			return false;
		}

		while (avcodec_receive_packet(cc, &pkt) == 0) {
			m->ret = write_frame(cc, fc, &st->time_base, st, &pkt);
		}

		if (flush) {
			m->video_is_eof = true;
		}
	}
	if (m->ret < 0) {
		exit(1);
	}
	m->video_pts = m->video_frame->pts;
	m->frame_count++;
	return true;
}

void VideoEncoder::close_video()
{
	if (m->video_codec_context) {
		avcodec_close(m->video_codec_context);
		avcodec_free_context(&m->video_codec_context);
	}
	m->src_picture.free();
	m->dst_picture.free();
	av_frame_free(&m->video_frame);
}

bool VideoEncoder::is_recording() const
{
	return (m->video_st && !m->video_is_eof) || (m->audio_st && !m->audio_is_eof);
}

namespace {
AVStream *add_stream(AVFormatContext *fc, AVCodecContext **cc, AVCodec const **codec, AVCodecID codec_id, VideoEncoder::AudioOption const &aopt, VideoEncoder::VideoOption const &vopt)
{
	AVCodecParameters *cp;
	AVStream *st;

	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		return nullptr;
	}

	st = avformat_new_stream(fc, *codec);
	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		return nullptr;
	}

	*cc = avcodec_alloc_context3(*codec);

	st->id = fc->nb_streams - 1;
	cp = st->codecpar;
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		(*cc)->channels = 2;
		(*cc)->sample_rate = aopt.sample_rate;
		(*cc)->bit_rate = 160000;
		(*cc)->sample_fmt = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		(*cc)->channel_layout = av_get_default_channel_layout((*cc)->channels);
		break;
	case AVMEDIA_TYPE_VIDEO:
		(*cc)->width = vopt.width;
		(*cc)->height = vopt.height;
		(*cc)->pix_fmt = AV_PIX_FMT_YUV420P;
		(*cc)->time_base.num = 100;
		(*cc)->time_base.den = vopt.fps * (*cc)->time_base.num;
		(*cc)->bit_rate = cp->width * cp->height * 8;

		(*cc)->gop_size = 12;
		(*cc)->field_order = AV_FIELD_PROGRESSIVE;
		(*cc)->color_range = AVCOL_RANGE_MPEG;
		(*cc)->color_primaries = AVCOL_PRI_BT709;
		(*cc)->color_trc = AVCOL_TRC_BT709;
		(*cc)->colorspace = AVCOL_SPC_BT709;
		(*cc)->chroma_sample_location = AVCHROMA_LOC_LEFT;
		(*cc)->sample_aspect_ratio = {1, 1};

		if ((*cc)->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			(*cc)->max_b_frames = 2;
		}
		if ((*cc)->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			(*cc)->mb_decision = 2;
		}
		if (fc->flags & AVFMT_GLOBALHEADER) {
			(*cc)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}
		break;
	}
	avcodec_parameters_from_context(cp, *cc);
	st->time_base = (*cc)->time_base;
	return st;
}
}

void VideoEncoder::run()
{
	AVOutputFormat const *fmt;
	AVFormatContext *fc;
	AVCodec const *audio_codec;
	AVCodec const *video_codec;
	double audio_time, video_time;

	av_log_set_level(AV_LOG_WARNING);

	std::string ext;
	{
		auto pos = m->filepath.find_last_of('.');
		if (pos != std::string::npos) {
			ext = m->filepath.substr(pos + 1);
		}
	}
	AVOutputFormat oformat = *av_guess_format(ext.c_str(), m->filepath.c_str(), nullptr);
	oformat.flags |= AVFMT_TS_NONSTRICT;
//	oformat.video_codec = AV_CODEC_ID_MPEG4;

	AVIOContext *io_context = nullptr;
	if (!(oformat.flags & AVFMT_NOFILE)) {
		m->ret = avio_open(&io_context, m->filepath.c_str(), AVIO_FLAG_WRITE);
		if (m->ret < 0) return;
	}

	m->ret = avformat_alloc_output_context2(&fc, &oformat, nullptr, nullptr);
	if (!fc) {
		return;
	}
	fc->pb = io_context;

	fmt = fc->oformat;

	m->video_st = nullptr;
	m->audio_st = nullptr;
	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		m->video_st = add_stream(fc, &m->video_codec_context, &video_codec, fmt->video_codec, m->aopt, m->vopt);
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		m->audio_st = add_stream(fc, &m->audio_codec_context, &audio_codec, fmt->audio_codec, m->aopt, m->vopt);
	}

	if (m->video_st && !open_video(m->video_codec_context, video_codec, m->video_st)) return;
	if (m->audio_st && !open_audio(m->audio_codec_context, audio_codec, m->audio_st)) return;

	m->ret = avformat_write_header(fc, nullptr);
	if (m->ret < 0) {
		fprintf(stderr, "avformat_write_header failed\n");
		return;
	}

	bool flush = false;
	while (is_recording()) {
		if (is_interruption_requested()) {
			flush = true;
			break;
		}

		audio_time = (m->audio_st && !m->audio_is_eof) ? m->audio_pts * av_q2d(m->audio_st->time_base) : INFINITY;
		video_time = (m->video_st && !m->video_is_eof) ? m->video_pts * av_q2d(m->video_codec_context->time_base) : INFINITY;

		bool f = false;
		if (m->audio_st && !m->audio_is_eof && audio_time <= video_time) {
			f = next_audio_frame(m->audio_codec_context, fc, m->audio_st, flush);
		} else if (m->video_st && !m->video_is_eof && video_time < audio_time) {
			f = next_video_frame(m->video_codec_context, fc, m->video_st, flush);
		}
		if (!f) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	av_write_trailer(fc);
	if (!(fmt->flags & AVFMT_NOFILE)) {
		avio_close(fc->pb);
	}

	close_video();
	close_audio();
	avformat_free_context(fc);
	m->video_st = nullptr;
	m->audio_st = nullptr;
	m->ret = 0;
}

void VideoEncoder::open(std::string const &filepath, const VideoOption &vopt, const AudioOption &aopt)
{
	m->filepath = filepath;
	m->vopt = vopt;
	m->aopt = aopt;

	std::thread th([&](){
		run();
	});

	m->thread = std::move(th);
}

void VideoEncoder::close()
{
	request_interruption();
	if (m->thread.joinable()) {
		m->thread.join();
	}
}

bool VideoEncoder::put_video_frame(const VideoFrame &img)
{
	if (img) {
		std::lock_guard lock(m->mutex);
		if (is_recording()) {
			m->video_frames.push_back(img);
			fprintf(stderr, "video queue:%d\n", m->video_frames.size());
			while (m->video_frames.size() > 100) {
				m->video_frames.pop_front();
			}
			return true;
		}
	}
	return false;
}

bool VideoEncoder::put_audio_frame(AudioFrame const &pcm)
{
	std::lock_guard lock(m->mutex);
	if (is_recording()) {
		m->audio_frames.push_back(pcm);
		fprintf(stderr, "audio queue:%d\n", m->audio_frames.size());
		while (m->audio_frames.size() > 100) {
			m->audio_frames.pop_front();
		}
		return true;
	}
	return false;
}

