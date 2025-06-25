#include "VideoFrameData.h"
#include "FFmpegVideoEncoder.h"
#include <assert.h>
#include <condition_variable>
#include <deque>
#include <cmath>
#include <mutex>
#include <thread>

#ifdef USE_FFMPEG
#include "includeffmpeg.h"
#endif

using namespace VideoEncoderOption;
using namespace VideoEncoderInternal;

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

class FFmpegVideoEncoder::MyPicture {
public:
	int width = 0;
	int height = 0;
	uint8_t *pointers[4] = {};
	int linesize[4] = {};

	int alloc(int width, int height, int pixfmt)
	{
		this->width = width;
		this->height = height;
		int r = av_image_alloc(pointers, linesize, width, height, (AVPixelFormat)pixfmt, 1);
		if (r < 0) {
			fprintf(stderr, "av_image_alloc failed\n");
		}
		return r;
	}
	void free()
	{
		av_freep(pointers);
	}
};

struct FFmpegVideoEncoder::Private {
	std::mutex mutex;
	std::condition_variable cond;

	Format format = Format::MPEG4;

	std::string filepath;
	VideoOption vopt;
	AudioOption aopt;

	bool recording_ready = false;
	bool is_audio_recording = false;
	bool is_video_recording = false;
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

	AVFormatContext *fc = nullptr;
	AVCodecContext *video_codec_context = nullptr;
	AVCodecContext *audio_codec_context = nullptr;
	AVFrame *video_frame = nullptr;
	FFmpegVideoEncoder::MyPicture src_picture;
	FFmpegVideoEncoder::MyPicture dst_picture;
	int frame_count = 0;
	AVStream *audio_st = nullptr;
	AVStream *video_st = nullptr;

	std::deque<VideoFrame> input_video_frames;
	std::deque<AudioFrame> input_audio_frames;

	std::thread thread;
	int ret = 0;
	bool interrupted = false;
};

FFmpegVideoEncoder::FFmpegVideoEncoder()
	: m(new Private)
{
}

FFmpegVideoEncoder::~FFmpegVideoEncoder()
{
	close();
	delete m;
}

void FFmpegVideoEncoder::request_interruption()
{
	m->recording_ready = false;
	m->interrupted = true;
}

bool FFmpegVideoEncoder::is_recording() const
{
	return m->recording_ready && (m->is_audio_recording || m->is_video_recording);
}

bool FFmpegVideoEncoder::is_interruption_requested() const
{
	return m->interrupted;
}

void FFmpegVideoEncoder::default_get_audio_frame(int16_t *samples, int frame_size, int nb_channels)
{
	while (frame_size > 0) {
		std::lock_guard lock(m->mutex);
		if (m->input_audio_frames.empty()) break;
		AudioFrame *frame = &m->input_audio_frames.front();
		int n = frame->samples.size() / nb_channels / sizeof(int16_t);
		n = std::min(n, frame_size);
		frame_size -= n;
		n *= nb_channels * sizeof(int16_t);
		memcpy(samples, frame->samples.data(), n);
		samples += n / sizeof(int16_t);
		if (frame->samples.size() < n + nb_channels * sizeof(int16_t)) {
			m->input_audio_frames.pop_front();
		} else {
			frame->samples.remove(0, n);
		}
	}
	int n = frame_size * nb_channels;
	for (int i = 0; i < n; i++) {
		samples[i] = 0;
	}
}

bool FFmpegVideoEncoder::get_audio_frame(int16_t *samples, int frame_size, int nb_channels)
{
	AudioFrame frame;
	default_get_audio_frame(samples, frame_size, nb_channels);
	return true;
}

bool FFmpegVideoEncoder::open_audio(AVCodecContext *cc, AVCodec const *codec, AVStream *st, const AudioOption &opt)
{
	AVCodecParameters *cp = st->codecpar;

	m->audio_frame = av_frame_alloc();
	if (!m->audio_frame) {
		fprintf(stderr, "Could not allocate audio frame\n");
		return false;
	}
	m->audio_frame->format = cp->format;

	av_channel_layout_copy(&m->audio_frame->ch_layout, &cp->ch_layout);
	av_channel_layout_copy(&cc->ch_layout, &cp->ch_layout);


	// open
	m->ret = avcodec_open2(cc, codec, nullptr);
	if (m->ret < 0) {
		return false;
	}
	m->ret = avcodec_parameters_from_context(cp, cc);

	cp->frame_size = cc->frame_size;
	m->src_nb_samples = cc->frame_size;
	const int channels = cp->ch_layout.nb_channels;
	m->ret = av_samples_alloc_array_and_samples(&m->src_samples_data, &m->src_samples_linesize, channels, m->src_nb_samples, AV_SAMPLE_FMT_S16, 0);
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
		av_opt_set_int       (m->swr_ctx, "in_channel_count",   channels,                   0);
		av_opt_set_int       (m->swr_ctx, "in_sample_rate",     cp->sample_rate,            0);
		av_opt_set_sample_fmt(m->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16,          0);
		av_opt_set_int       (m->swr_ctx, "out_channel_count",  channels,                   0);
		av_opt_set_int       (m->swr_ctx, "out_sample_rate",    cp->sample_rate,            0);
		av_opt_set_sample_fmt(m->swr_ctx, "out_sample_fmt",     (AVSampleFormat)cp->format, 0);
		/* initialize the resampling context */
		if ((m->ret = swr_init(m->swr_ctx)) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			return false;
		}
		m->ret = av_samples_alloc_array_and_samples(&m->dst_samples_data, &m->dst_samples_linesize, channels, m->max_dst_nb_samples, (AVSampleFormat)cp->format, 0);
		if (m->ret < 0) {
			fprintf(stderr, "Could not allocate destination samples\n");
			return false;
		}
	} else {
		m->dst_samples_data = m->src_samples_data;
	}
	m->dst_samples_size = av_samples_get_buffer_size(nullptr, channels, m->max_dst_nb_samples, (AVSampleFormat)cp->format, 0);
	return true;
}

bool FFmpegVideoEncoder::next_audio_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush)
{
	if (!st) return false;
	if (!m->audio_frame) return false;

	AVPacket pkt = {}; // data and size must be 0;
	int dst_nb_samples;
	AVCodecParameters *cp = st->codecpar;
	if (!flush) {
		int channels = cp->ch_layout.nb_channels;
		if (!get_audio_frame((int16_t *)m->src_samples_data[0], m->src_nb_samples, channels)) {
			return false;
		}
		/* convert samples from native format to destination codec format, using the resampler */
		if (m->swr_ctx) {
			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(m->swr_ctx, cp->sample_rate) + m->src_nb_samples, cp->sample_rate, cp->sample_rate, AV_ROUND_UP);
			if (dst_nb_samples > m->max_dst_nb_samples) {
				av_free(m->dst_samples_data[0]);
				m->ret = av_samples_alloc(m->dst_samples_data, &m->dst_samples_linesize, channels, dst_nb_samples, (AVSampleFormat)cp->format, 0);
				if (m->ret < 0) {
					return false;
				}
				m->max_dst_nb_samples = dst_nb_samples;
				m->dst_samples_size = av_samples_get_buffer_size(nullptr, channels, dst_nb_samples, (AVSampleFormat)cp->format, 0);
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
		m->ret = avcodec_fill_audio_frame(m->audio_frame, channels, (AVSampleFormat)cp->format, m->dst_samples_data[0], m->dst_samples_size, 0);
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

void FFmpegVideoEncoder::close_audio()
{
	if (m->audio_codec_context) {
		avcodec_close(m->audio_codec_context);
		avcodec_free_context(&m->audio_codec_context);
	}
	if (m->dst_samples_data) {
		if (m->dst_samples_data != m->src_samples_data) {
			av_free(m->dst_samples_data[0]);
			av_free(m->dst_samples_data);
		}
	}
	if (m->src_samples_data) {
		av_free(m->src_samples_data[0]);
		av_free(m->src_samples_data);
	}
	av_frame_free(&m->audio_frame);
}

void FFmpegVideoEncoder::default_get_video_frame(VideoFrame *out)
{
	std::lock_guard lock(m->mutex);
	if (!m->input_video_frames.empty()) {
		std::swap(*out, m->input_video_frames.front());
		m->input_video_frames.pop_front();
	}
}

bool FFmpegVideoEncoder::get_video_frame(MyPicture *pict)
{
	VideoFrame frame;
	default_get_video_frame(&frame);
	if (frame.width() != pict->width || frame.height() != pict->height) return false;

	frame.image = frame.image.convertToFormat(Image::Format::YUYV8);
	uint8_t *p = pict->pointers[0];
	memcpy(p, frame.image.bits(), frame.image.bytesPerLine() * frame.height());

	return true;
}

bool FFmpegVideoEncoder::open_video(AVCodecContext *cc, AVCodec const *codec, AVStream *st, VideoOption const &opt)
{
	AVCodecParameters *c = st->codecpar;

	AVDictionary *codec_options = nullptr;

	m->ret = avcodec_open2(cc, codec, &codec_options);
	if (m->ret < 0) {
		fprintf(stderr, "avcodec_open2 failed\n");
		return false;
	}

	m->ret = avcodec_parameters_from_context(st->codecpar, cc);

	m->video_frame = av_frame_alloc();
	if (!m->video_frame) {
		fprintf(stderr, "av_frame_alloc failed\n");
		return false;
	}
	m->video_frame->format = c->format;
	m->video_frame->width = c->width;
	m->video_frame->height = c->height;

	m->ret = m->dst_picture.alloc(c->width, c->height, c->format);
	if (m->ret < 0) return false;

	m->ret = m->src_picture.alloc(opt.src_w, opt.src_h, m->source_pixel_format);
	if (m->ret < 0) return false;

	for (int i = 0; i < 4; i++) {
		m->video_frame->data[i] = m->dst_picture.pointers[i];
		m->video_frame->linesize[i] = m->dst_picture.linesize[i];
	}

	return true;
}

bool FFmpegVideoEncoder::next_video_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush)
{
	if (!st) return false;
	if (!m->video_frame) return false;

	AVCodecParameters *c = st->codecpar;
	if (!flush) {
		if (!m->sws_ctx) {
			m->sws_ctx = sws_getContext(m->src_picture.width, m->src_picture.height, m->source_pixel_format, c->width, c->height, (AVPixelFormat)c->format, SWS_BILINEAR, nullptr, nullptr, nullptr);
			if (!m->sws_ctx) {
				fprintf(stderr, "Could not initialize the conversion context\n");
				exit(1);
			}
		}
		if (!get_video_frame(&m->src_picture)) {
			return false;
		}
		sws_scale(m->sws_ctx, (const uint8_t * const *)m->src_picture.pointers, m->src_picture.linesize, 0, m->src_picture.height, m->dst_picture.pointers, m->dst_picture.linesize);
	}
	{
		AVPacket pkt = {};

		m->video_frame->pts = m->frame_count;

		m->ret = avcodec_send_frame(cc, flush ? nullptr : m->video_frame);
		if (m->ret < 0 && m->ret != AVERROR_EOF) {
			fprintf(stderr, "avcodec_send_frame failed\n");
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
		print_averror(m->ret);
		exit(1);
	}
	m->video_pts = m->video_frame->pts;
	m->frame_count++;
	return true;
}

void FFmpegVideoEncoder::close_video()
{
	if (m->video_codec_context) {
		avcodec_close(m->video_codec_context);
		avcodec_free_context(&m->video_codec_context);
	}
	m->src_picture.free();
	m->dst_picture.free();
	av_frame_free(&m->video_frame);
}

namespace {

AVCodecContext *new_codec_context(AVFormatContext *fc, AVCodec const *codec, AudioOption const &aopt, VideoOption const &vopt)
{
	AVCodecContext *cc = avcodec_alloc_context3(codec);
	switch (codec->type) {
	case AVMEDIA_TYPE_AUDIO:
		cc->sample_rate = aopt.sample_rate;
		cc->bit_rate = 160000;
		cc->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		break;
	case AVMEDIA_TYPE_VIDEO:
		cc->width = vopt.dst_w;
		cc->height = vopt.dst_h;
		cc->pix_fmt = AV_PIX_FMT_YUV420P;
		cc->time_base.num = vopt.fps.den;
		cc->time_base.den = vopt.fps.num;
		cc->bit_rate = cc->width * cc->height * 8;

		cc->gop_size = 12;
		cc->field_order = AV_FIELD_PROGRESSIVE;
		cc->sample_aspect_ratio = {1, 1};

		if (cc->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			cc->max_b_frames = 2;
		}
		if (cc->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			cc->mb_decision = 2;
		}
		if (fc->flags & AVFMT_GLOBALHEADER) {
			cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}
		break;
	case AVMEDIA_TYPE_UNKNOWN:
	case AVMEDIA_TYPE_DATA:
	case AVMEDIA_TYPE_SUBTITLE:
	case AVMEDIA_TYPE_ATTACHMENT:
	case AVMEDIA_TYPE_NB:
		break;
	}
	return cc;
}

AVStream *add_stream(AVFormatContext *fc, AVCodec const *codec, AVCodecContext const *cc, AudioOption const &aopt, VideoOption const &vopt)
{
	AVStream *st = avformat_new_stream(fc, codec);
	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		return nullptr;
	}

	if (!st->codecpar) {
		st->codecpar = avcodec_parameters_alloc();
	}

	st->id = fc->nb_streams - 1;
	st->time_base = cc->time_base;
	avcodec_parameters_from_context(st->codecpar, cc);
	return st;
}

} // namespace

void FFmpegVideoEncoder::run()
{
	m->recording_ready = true;

	bool flush = false;
	while ((m->is_video_recording && !m->video_is_eof) || (m->is_audio_recording && !m->audio_is_eof)) {
		double audio_time = (m->audio_st && !m->audio_is_eof) ? m->audio_pts * av_q2d(m->audio_st->time_base) : INFINITY;
		double video_time = (m->video_st && !m->video_is_eof) ? m->video_pts * av_q2d(m->video_codec_context->time_base) : INFINITY;

		bool f = false;
		if (m->audio_st && !m->audio_is_eof && audio_time <= video_time) {
			f = next_audio_frame(m->audio_codec_context, m->fc, m->audio_st, flush);
		} else if (m->video_st && !m->video_is_eof && video_time < audio_time) {
			f = next_video_frame(m->video_codec_context, m->fc, m->video_st, flush);
		}
		if (!f) {
			std::unique_lock lock(m->mutex);
			m->cond.wait(lock);
		}
	}

	m->recording_ready = false;

	av_write_trailer(m->fc);
	if (!(m->fc->oformat->flags & AVFMT_NOFILE)) {
		avio_close(m->fc->pb);
	}

	close_video();
	close_audio();
	avformat_free_context(m->fc);
	m->fc = nullptr;
	m->video_st = nullptr;
	m->audio_st = nullptr;
	m->ret = 0;
}

bool FFmpegVideoEncoder::create(std::string const &filepath, Format format, const VideoOption &vopt, const AudioOption &aopt)
{
	if (is_recording()) return false;

	m->recording_ready = false;

	m->filepath = filepath;
	m->format = format;
	m->vopt = vopt;
	m->aopt = aopt;
	m->is_video_recording = m->vopt.active;
	m->is_audio_recording = m->aopt.active;

	av_log_set_level(AV_LOG_INFO);

	std::string suffix;
	AVCodecID vcodec = AV_CODEC_ID_NONE;
	AVCodecID acodec = AV_CODEC_ID_NONE;
	std::string vcname;
	std::string acname;

	switch (m->format) {
	case Format::MPEG4:
		suffix = "mp4";
		vcodec = AV_CODEC_ID_MPEG4;
		acodec = AV_CODEC_ID_AC3;
		break;
	case Format::H264_NVENC:
		suffix = "mp4";
		vcodec = AV_CODEC_ID_H264;
		acodec = AV_CODEC_ID_AC3;
		vcname = "h264_nvenc";
		break;
	case Format::HEVC_NVENC:
		suffix = "mp4";
		vcodec = AV_CODEC_ID_HEVC;
		acodec = AV_CODEC_ID_AC3;
		vcname = "hevc_nvenc";
		break;
	case Format::LIBSVTAV1:
		suffix = "mkv";
		vcodec = AV_CODEC_ID_AV1;
		acodec = AV_CODEC_ID_AC3;
		vcname = "libsvtav1";
		break;
	}

	AVOutputFormat const *oformat = av_guess_format(suffix.c_str(), nullptr, nullptr);
	if (!oformat) {
		fprintf(stderr, "format not supported\n");
		return false;
	}

	AVCodec const *audio_codec = nullptr;
	AVCodec const *video_codec = nullptr;

	if (m->vopt.active) {
		if (!vcname.empty()) {
			video_codec = avcodec_find_encoder_by_name(vcname.c_str());
		}
		if (!video_codec) {
			video_codec = avcodec_find_encoder(vcodec);
		}
	}

	if (m->aopt.active) {
		if (!acname.empty()) {
			audio_codec = avcodec_find_encoder_by_name(acname.c_str());
		}
		if (!audio_codec) {
			audio_codec = avcodec_find_encoder(acodec);
		}
	}

	AVIOContext *io_context = nullptr;
	if (!(oformat->flags & AVFMT_NOFILE)) {
		m->ret = avio_open(&io_context, m->filepath.c_str(), AVIO_FLAG_WRITE);
		if (m->ret < 0) {
			fprintf(stderr, "avio_open failed\n");
			return false;
		}
	}

	m->ret = avformat_alloc_output_context2(&m->fc, oformat, nullptr, nullptr);
	if (!m->fc) {
		fprintf(stderr, "avformat_alloc_output_context2 failed\n");
		return false;
	}

	m->video_st = nullptr;
	if (video_codec) {
		m->video_codec_context = new_codec_context(m->fc, video_codec, m->aopt, m->vopt);
		m->video_st = add_stream(m->fc, video_codec, m->video_codec_context, m->aopt, m->vopt);
	}

	m->audio_st = nullptr;
	if (audio_codec) {
		m->audio_codec_context = new_codec_context(m->fc, audio_codec, m->aopt, m->vopt);;
		m->audio_st = add_stream(m->fc, audio_codec, m->audio_codec_context, m->aopt, m->vopt);
		av_channel_layout_default(&m->audio_st->codecpar->ch_layout, m->aopt.channels);
	}

	if (m->video_st && !open_video(m->video_codec_context, video_codec, m->video_st, m->vopt)) return false;
	if (m->audio_st && !open_audio(m->audio_codec_context, audio_codec, m->audio_st, m->aopt)) return false;

	m->fc->pb = io_context;
	m->fc->flags |= AVFMT_GLOBALHEADER;
	m->ret = avformat_write_header(m->fc, nullptr);
	if (m->ret < 0) {
		fprintf(stderr, "avformat_write_header failed\n");
		return false;
	}

	std::thread th([&](){ // start recording thread
		run();
	});

	m->thread = std::move(th);
	return true;
}

void FFmpegVideoEncoder::close()
{
	m->is_video_recording = false;
	m->is_audio_recording = false;
	m->cond.notify_all();
	if (m->thread.joinable()) {
		m->thread.join();
	}
}

bool FFmpegVideoEncoder::put_video_frame(const VideoFrame &img)
{
	if (img) {
		std::lock_guard lock(m->mutex);
		if (m->is_video_recording) {
			m->input_video_frames.push_back(img);
//			fprintf(stderr, "video queue:%d\n", m->input_video_frames.size());
			if (m->vopt.drop_if_overflow) {
				while (m->input_video_frames.size() > 100) {
					m->input_video_frames.pop_front();
				}
			}
			m->cond.notify_all();
			return true;
		}
	}
	return false;
}

bool FFmpegVideoEncoder::put_audio_frame(AudioFrame const &pcm)
{
	if (pcm) {
		std::lock_guard lock(m->mutex);
		if (m->is_audio_recording) {
			m->input_audio_frames.push_back(pcm);
//			fprintf(stderr, "audio queue:%d\n", m->input_audio_frames.size());
			if (m->aopt.drop_if_overflow) {
				while (m->input_audio_frames.size() > 100) {
					m->input_audio_frames.pop_front();
				}
			}
			m->cond.notify_all();
			return true;
		}
	}
	return false;
}

void FFmpegVideoEncoder::put_frame(const VideoFrameData &frame)
{
	if (!m->recording_ready) return;

	VideoFrame v;
	v.image = frame.d->image;
	put_video_frame(v);

	AudioFrame a;
	a.samples = frame.d->audio;
	put_audio_frame(a);
}

const AudioOption *FFmpegVideoEncoder::audio_option() const
{
	return &m->aopt;
}

const VideoOption *FFmpegVideoEncoder::video_option() const
{
	return &m->vopt;
}

