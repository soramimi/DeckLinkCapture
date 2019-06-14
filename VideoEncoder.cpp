#include "VideoEncoder.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <QDebug>
#include <QImage>
#include <QMutex>
#include <deque>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace {
//static const int STREAM_DURATION = 5.0;
//static const double STREAM_FRAME_RATE = 29.97;
static const AVPixelFormat STREAM_PIX_FMT = AV_PIX_FMT_YUV420P;
static const int sws_flags = SWS_BILINEAR;

int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
	pkt->stream_index = st->index;
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, AVCodecID codec_id, VideoEncoder::AudioOption const &aopt, VideoEncoder::VideoOption const &vopt)
{
	AVCodecContext *c;
	AVStream *st;
	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}
	st = avformat_new_stream(oc, *codec);
	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	st->id = oc->nb_streams - 1;
	c = st->codec;
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate    = 160000;
		c->sample_rate = 48000;
		c->channels    = 2;
		break;
	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		/* Resolution must be a multiple of two. */
		c->width    = 1920;//1280;
		c->height   = 1080;//720;
		c->bit_rate = c->width * c->height * 8;//8000000;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
#if 0
		c->time_base.num = 100;
		c->time_base.den = STREAM_FRAME_RATE * c->time_base.num;
#else
		c->time_base.num = 100;
		c->time_base.den = vopt.fps * c->time_base.num;
#endif
		c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt       = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		break;
	default:
		break;
	}
	/* Some formats want stream headers to be separate. */
//	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
//		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
//	}
	return st;
}

} // namespace

struct VideoEncoder::Private {
	QString filepath;
	VideoEncoder::VideoOption vopt;
	VideoEncoder::AudioOption aopt;

	bool audio_is_eof = false;
	bool video_is_eof = false;

	float t = 0;
	float tincr = 0;
	float tincr2 = 0;
	AVFrame *audio_frame = nullptr;
	uint8_t **src_samples_data = 0;
	int src_samples_linesize = 0;
	int src_nb_samples = 0;
	int max_dst_nb_samples = 0;
	uint8_t **dst_samples_data = nullptr;
	int dst_samples_linesize = 0;
	int dst_samples_size = 0;
	int samples_count = 0;
	struct SwrContext *swr_ctx = nullptr;

	double audio_pts = 0;
	double video_pts = 0;

	AVFrame *frame = nullptr;
	AVPicture src_picture;
	AVPicture dst_picture;
	int frame_count = 0;
	AVStream *audio_st = nullptr;
	AVStream *video_st = nullptr;

	QMutex mutex;
	std::deque<QImage> video_frames;
	std::deque<QByteArray> audio_frames;
};

VideoEncoder::VideoEncoder()
	: m(new Private)
{
}

VideoEncoder::~VideoEncoder()
{
	delete m;
}

bool VideoEncoder::get_audio_frame(int16_t *samples, int frame_size, int nb_channels)
{
#if 0
	int j, i, v;
	int16_t *q;
	q = samples;
	for (j = 0; j < frame_size; j++) {
		v = (int)(sin(m->t) * 10000);
		for (i = 0; i < nb_channels; i++) {
			*q++ = v;
		}
		m->t     += m->tincr;
		m->tincr += m->tincr2;
	}
#else
	while (frame_size > 0) {
		QMutexLocker lock(&m->mutex);
		if (m->audio_frames.empty()) break;
		QByteArray &ba = m->audio_frames.front();
		int n = ba.size() / nb_channels / sizeof(int16_t);
		n = std::min(n, frame_size);
		frame_size -= n;
		n *= nb_channels * sizeof(int16_t);
		memcpy(samples, ba.data(), n);
		samples += n / sizeof(int16_t);
		if (ba.size() < n + nb_channels * sizeof(int16_t)) {
			m->audio_frames.pop_front();
		} else {
			ba.remove(0, n);
		}
	}
#endif
	return true;
}

bool VideoEncoder::get_video_frame(AVPicture *pict, int frame_index, int width, int height)
{
#if 0
	int x, y, i;
	i = frame_index;
	for (y = 0; y < height; y++) {
		uint8_t *p = &pict->data[0][y * pict->linesize[0]];
		for (x = 0; x < width; x++) {
			int r = x * 255 / width;
			int g = y * 255 / height;
			int b = (((x + i) ^ (y + i)) & 64) ? 0 : 255;
			p[0] = r;
			p[1] = g;
			p[2] = b;
			p += 3;
		}
	}
#else
	{
		QImage img;
		{
			QMutexLocker lock(&m->mutex);
			if (m->video_frames.empty()) {
				return false;
			}
			img = m->video_frames.front();
			m->video_frames.pop_front();
		}
		if (img.isNull()) return false;

		img = img.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		img = img.convertToFormat(QImage::Format_RGB888);
		uint8_t *p = &pict->data[0][0];
		memcpy(p, img.bits(), width * height * 3);
	}
#endif
	return true;
}

void VideoEncoder::open_audio(AVFormatContext *oc, AVCodec *codec, AVStream *st)
{
	AVCodecContext *c;
	int ret;
	c = st->codec;
	/* allocate and init a re-usable frame */
	m->audio_frame = av_frame_alloc();
	if (!m->audio_frame) {
		fprintf(stderr, "Could not allocate audio frame\n");
		exit(1);
	}
	/* open it */
	c->strict_std_compliance = oc->strict_std_compliance;
	ret = avcodec_open2(c, codec, nullptr);
	if (ret < 0) {
		//		fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
		exit(1);
	}
	/* init signal generator */
	m->t     = 0;
	m->tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	m->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;
	m->src_nb_samples = c->frame_size;
	ret = av_samples_alloc_array_and_samples(&m->src_samples_data, &m->src_samples_linesize, c->channels, m->src_nb_samples, AV_SAMPLE_FMT_S16, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate source samples\n");
		exit(1);
	}
	/* compute the number of converted samples: buffering is avoided
	 * ensuring that the output buffer will contain at least all the
	 * converted input samples */
	m->max_dst_nb_samples = m->src_nb_samples;
	/* create resampler context */
	if (c->sample_fmt != AV_SAMPLE_FMT_S16) {
		m->swr_ctx = swr_alloc();
		if (!m->swr_ctx) {
			fprintf(stderr, "Could not allocate resampler context\n");
			exit(1);
		}
		/* set options */
		av_opt_set_int       (m->swr_ctx, "in_channel_count",   c->channels,       0);
		av_opt_set_int       (m->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
		av_opt_set_sample_fmt(m->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
		av_opt_set_int       (m->swr_ctx, "out_channel_count",  c->channels,       0);
		av_opt_set_int       (m->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
		av_opt_set_sample_fmt(m->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
		/* initialize the resampling context */
		if ((ret = swr_init(m->swr_ctx)) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			exit(1);
		}
		ret = av_samples_alloc_array_and_samples(&m->dst_samples_data, &m->dst_samples_linesize, c->channels, m->max_dst_nb_samples, c->sample_fmt, 0);
		if (ret < 0) {
			fprintf(stderr, "Could not allocate destination samples\n");
			exit(1);
		}
	} else {
		m->dst_samples_data = m->src_samples_data;
	}
	m->dst_samples_size = av_samples_get_buffer_size(nullptr, c->channels, m->max_dst_nb_samples, c->sample_fmt, 0);
}

void VideoEncoder::write_audio_frame(AVFormatContext *oc, AVStream *st, bool flush)
{
	AVCodecContext *c;
	AVPacket pkt = {}; // data and size must be 0;
	int got_packet, ret, dst_nb_samples;
	av_init_packet(&pkt);
	c = st->codec;
	if (!flush) {
		if (!get_audio_frame((int16_t *)m->src_samples_data[0], m->src_nb_samples, c->channels)) {
			return;
		}
		/* convert samples from native format to destination codec format, using the resampler */
		if (m->swr_ctx) {
			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(m->swr_ctx, c->sample_rate) + m->src_nb_samples, c->sample_rate, c->sample_rate, AV_ROUND_UP);
			if (dst_nb_samples > m->max_dst_nb_samples) {
				av_free(m->dst_samples_data[0]);
				ret = av_samples_alloc(m->dst_samples_data, &m->dst_samples_linesize, c->channels, dst_nb_samples, c->sample_fmt, 0);
				if (ret < 0) {
					exit(1);
				}
				m->max_dst_nb_samples = dst_nb_samples;
				m->dst_samples_size = av_samples_get_buffer_size(nullptr, c->channels, dst_nb_samples, c->sample_fmt, 0);
			}
			/* convert to destination format */
			ret = swr_convert(m->swr_ctx, m->dst_samples_data, dst_nb_samples, (const uint8_t **)m->src_samples_data, m->src_nb_samples);
			if (ret < 0) {
				fprintf(stderr, "Error while converting\n");
				exit(1);
			}
		} else {
			dst_nb_samples = m->src_nb_samples;
		}
		m->audio_frame->nb_samples = dst_nb_samples;
		AVRational rate = { 1, c->sample_rate };
		m->audio_frame->pts = av_rescale_q(m->samples_count, rate, c->time_base);
		avcodec_fill_audio_frame(m->audio_frame, c->channels, c->sample_fmt, m->dst_samples_data[0], m->dst_samples_size, 0);
		m->samples_count += dst_nb_samples;
	}
	ret = avcodec_encode_audio2(c, &pkt, flush ? nullptr : m->audio_frame, &got_packet);
	if (ret < 0) {
		//		fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
		exit(1);
	}
	if (!got_packet) {
		if (flush) {
			m->audio_is_eof = true;
		}
		return;
	}
	ret = write_frame(oc, &c->time_base, st, &pkt);
	if (ret < 0) {
		//		fprintf(stderr, "Error while writing audio frame: %s\n", av_err2str(ret));
		exit(1);
	}
	m->audio_pts = (double)m->samples_count * st->time_base.den / st->time_base.num / c->sample_rate;
}

void VideoEncoder::close_audio(AVFormatContext *oc, AVStream *st)
{
	(void)oc;
	avcodec_close(st->codec);
	if (m->dst_samples_data != m->src_samples_data) {
		av_free(m->dst_samples_data[0]);
		av_free(m->dst_samples_data);
	}
	av_free(m->src_samples_data[0]);
	av_free(m->src_samples_data);
	av_frame_free(&m->audio_frame);
}

void VideoEncoder::open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st)
{
	(void)oc;
	int ret;
	AVCodecContext *c = st->codec;
	/* open the codec */
	ret = avcodec_open2(c, codec, nullptr);
	if (ret < 0) {
		//		fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
		exit(1);
	}
	/* allocate and init a re-usable frame */
	m->frame = av_frame_alloc();
	if (!m->frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	m->frame->format = c->pix_fmt;
	m->frame->width = c->width;
	m->frame->height = c->height;
	/* Allocate the encoded raw picture. */
	ret = avpicture_alloc(&m->dst_picture, c->pix_fmt, c->width, c->height);
	if (ret < 0) {
		//		fprintf(stderr, "Could not allocate picture: %s\n", av_err2str(ret));
		exit(1);
	}
	ret = avpicture_alloc(&m->src_picture, AV_PIX_FMT_RGB24, c->width, c->height);
	if (ret < 0) {
		//			fprintf(stderr, "Could not allocate temporary picture: %s\n", av_err2str(ret));
		exit(1);
	}
	/* copy data and linesize picture pointers to frame */
	*((AVPicture *)m->frame) = m->dst_picture;
}

void VideoEncoder::write_video_frame(AVFormatContext *oc, AVStream *st, bool flush)
{
	int ret;
	static struct SwsContext *sws_ctx;
	AVCodecContext *c = st->codec;
	if (!flush) {
		/* as we only generate a YUV420P picture, we must convert it
		 * to the codec pixel format if needed */
		if (!sws_ctx) {
			sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_RGB24, c->width, c->height, c->pix_fmt, sws_flags, nullptr, nullptr, nullptr);
			if (!sws_ctx) {
				fprintf(stderr, "Could not initialize the conversion context\n");
				exit(1);
			}
		}
		if (!get_video_frame(&m->src_picture, m->frame_count, c->width, c->height)) {
			return;
		}
		sws_scale(sws_ctx, (const uint8_t * const *)m->src_picture.data, m->src_picture.linesize, 0, c->height, m->dst_picture.data, m->dst_picture.linesize);
	}
//	if ((oc->oformat->flags & AVFMT_RAWPICTURE) && !flush) {
//		/* Raw video case - directly store the picture in the packet */
//		AVPacket pkt;
//		av_init_packet(&pkt);
//		pkt.flags        |= AV_PKT_FLAG_KEY;
//		pkt.stream_index  = st->index;
//		pkt.data          = m->dst_picture.data[0];
//		pkt.size          = sizeof(AVPicture);
//		ret = av_interleaved_write_frame(oc, &pkt);
//	} else
	{
		AVPacket pkt = {};
		int got_packet;
		av_init_packet(&pkt);
		/* encode the image */
		m->frame->pts = m->frame_count;
		ret = avcodec_encode_video2(c, &pkt, flush ? nullptr : m->frame, &got_packet);
		if (ret < 0) {
			//			fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
			exit(1);
		}
		/* If size is zero, it means the image was buffered. */
		if (got_packet) {
			ret = write_frame(oc, &c->time_base, st, &pkt);
		} else {
			if (flush) {
				m->video_is_eof = true;
			}
			ret = 0;
		}
	}
	if (ret < 0) {
		//		fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
		exit(1);
	}
	m->video_pts = m->frame->pts;
	m->frame_count++;
}

void VideoEncoder::close_video(AVFormatContext *oc, AVStream *st)
{
	(void)oc;
	avcodec_close(st->codec);
	av_free(m->src_picture.data[0]);
	av_free(m->dst_picture.data[0]);
	av_frame_free(&m->frame);
}

bool VideoEncoder::is_recording() const
{
	return (m->video_st && !m->video_is_eof) || (m->audio_st && !m->audio_is_eof);
}

int VideoEncoder::save()
{
	std::string filename = m->filepath.toStdString();
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVCodec *audio_codec, *video_codec;
	double audio_time, video_time;
	int ret;

	//	av_log_set_level(AV_LOG_ERROR);
	av_log_set_level(AV_LOG_WARNING);

	/* Initialize libavcodec, and register all codecs and formats. */
	av_register_all();
	/* allocate the output media context */
	avformat_alloc_output_context2(&oc, nullptr, nullptr, filename.c_str());
	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, nullptr, "avi", filename.c_str());
	}
	if (!oc) return 1;
	//	oc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	fmt = oc->oformat;
	assert(fmt->audio_codec == AV_CODEC_ID_MP3);
	assert(fmt->video_codec == AV_CODEC_ID_MPEG4);
	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	m->video_st = nullptr;
	m->audio_st = nullptr;
	if (fmt->video_codec != AV_CODEC_ID_NONE) m->video_st = add_stream(oc, &video_codec, fmt->video_codec, m->aopt, m->vopt);
	if (fmt->audio_codec != AV_CODEC_ID_NONE) m->audio_st = add_stream(oc, &audio_codec, fmt->audio_codec, m->aopt, m->vopt);
	/* Now that all the parameters are set, we can open the audio and
		 * video codecs and allocate the necessary encode buffers. */
	if (m->video_st) {
		open_video(oc, video_codec, m->video_st);
		m->video_st->time_base.den = 100 * m->vopt.fps;//STREAM_FRAME_RATE;
		m->video_st->time_base.num = 100;
	}
	if (m->audio_st) {
		open_audio(oc, audio_codec, m->audio_st);
	}
	av_dump_format(oc, 0, filename.c_str(), 1);
	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, filename.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			//			fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
			return 1;
		}
	}
	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, nullptr);
	if (ret < 0) {
		//		fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
		return 1;
	}
	bool flush = false;
	while (is_recording()) {
		if (isInterruptionRequested()) {
			flush = true;
		}
		/* Compute current audio and video time. */
		audio_time = (m->audio_st && !m->audio_is_eof) ? m->audio_pts * av_q2d(m->audio_st->time_base) : INFINITY;
		video_time = (m->video_st && !m->video_is_eof) ? m->video_pts * av_q2d(m->video_st->time_base) : INFINITY;
		//		audio_time = (audio_st && !audio_is_eof) ? audio_st->pts.val * av_q2d(audio_st->time_base) : INFINITY;
		//		video_time = (video_st && !video_is_eof) ? video_st->pts.val * av_q2d(video_st->time_base) : INFINITY;
//		if (!flush && (!m->audio_st || audio_time >= STREAM_DURATION) && (!m->video_st || video_time >= STREAM_DURATION)) {
//		}
		/* write interleaved audio and video frames */
		if (m->audio_st && !m->audio_is_eof && audio_time <= video_time) {
			write_audio_frame(oc, m->audio_st, flush);
			//			printf("A %f\n", audio_pts);
			//			putchar('A');
		} else if (m->video_st && !m->video_is_eof && video_time < audio_time) {
			write_video_frame(oc, m->video_st, flush);
			//			printf("V %f\n", video_pts);
			//			putchar('V');
		}
	}
	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	av_write_trailer(oc);
	/* Close each codec. */
	if (m->video_st) {
		close_video(oc, m->video_st);
		m->video_st = nullptr;
	}
	if (m->audio_st) {
		close_audio(oc, m->audio_st);
		m->audio_st = nullptr;
	}
	if (!(fmt->flags & AVFMT_NOFILE)) {
		/* Close the output file. */
		avio_close(oc->pb);
	}
	/* free the stream */
	avformat_free_context(oc);
	return 0;
}

void VideoEncoder::run()
{
	save();
}

void VideoEncoder::thread_start(const QString &filepath, const VideoOption &vopt, const AudioOption &aopt)
{
	m->filepath = filepath;
	m->vopt = vopt;
	m->aopt = aopt;
	start();
}

void VideoEncoder::thread_stop()
{
	requestInterruption();
	if (!wait(1000)) {
		terminate();
	}
}

bool VideoEncoder::putVideoFrame(const QImage &img)
{
	if (img.isNull()) return false;

	QMutexLocker lock(&m->mutex);
	if (is_recording()) {
		m->video_frames.push_back(img);
		return true;
	}
	return false;
}

bool VideoEncoder::putAudioFrame(const QByteArray &pcm)
{
	QMutexLocker lock(&m->mutex);
	if (is_recording()) {
		m->audio_frames.push_back(pcm);
		return true;
	}
	return false;
}

