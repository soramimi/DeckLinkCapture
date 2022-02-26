#include "Image.h"
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
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

void print_averror(int ret)
{
	QString msg = "AVERROR(%1)";
	msg = msg.arg(ret);
	if (ret < 0) {
		char tmp[1024];
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
	fprintf(stderr, "%s\n", msg.toStdString().c_str());
}

int MyPicture_alloc(MyPicture *picture, int width, int height, int pixfmt)
{
	return av_image_alloc(picture->pointers, picture->linesize, width, height, (AVPixelFormat)pixfmt, 1);
}

void MyPicture_free(MyPicture *picture)
{
	av_freep(picture->pointers);
}

namespace {
//static const int STREAM_DURATION = 5.0;
//static const double STREAM_FRAME_RATE = 29.97;
//static const AVPixelFormat STREAM_PIX_FMT = AV_PIX_FMT_YUV420P;
static const int sws_flags = SWS_BILINEAR;

int write_frame(AVCodecContext const *cc, AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
#if 0
	/* rescale output packet timestamp values from codec to stream timebase */
	pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
	pkt->stream_index = st->index;
#else
	av_packet_rescale_ts(pkt, cc->time_base, st->time_base);
	pkt->stream_index = st->index;
#endif
	return av_interleaved_write_frame(fmt_ctx, pkt);
//	return av_write_frame(fmt_ctx, pkt);
}

AVStream *add_stream(AVFormatContext *fc, AVCodecContext **cc, AVCodec const **codec, AVCodecID codec_id, VideoEncoder::AudioOption const &aopt, VideoEncoder::VideoOption const &vopt)
{
	AVCodecParameters *cp;
	AVStream *st;

	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		exit(1);
	}

	st = avformat_new_stream(fc, *codec);
	if (!st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}

	*cc = avcodec_alloc_context3(*codec);

	st->id = fc->nb_streams - 1;
	cp = st->codecpar;
	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
//		cp->format = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
//		cp->bit_rate    = 160000;
//		cp->sample_rate = 48000;
//		cp->channels    = 2;
		(*cc)->channels = 2;
		(*cc)->sample_rate = 48000;
		(*cc)->bit_rate = 160000;
		(*cc)->sample_fmt = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		(*cc)->channel_layout = av_get_default_channel_layout((*cc)->channels);
		break;
	case AVMEDIA_TYPE_VIDEO:
//		cp->codec_id = codec_id;
		/* Resolution must be a multiple of two. */
//		cp->width    = 1920;
//		cp->height   = 1080;
//		cp->format = AV_PIX_FMT_YUV420P;
		(*cc)->bit_rate = cp->width * cp->height * 8;//8000000;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
#if 0
		st->time_base.num = 100;
		st->time_base.den = STREAM_FRAME_RATE * st->time_base.num;
#else
		(*cc)->time_base.num = 100;
		(*cc)->time_base.den = vopt.fps * (*cc)->time_base.num;
//		qDebug() << st->time_base.den << st->time_base.num;
#endif
//		(*cc)->pix_fmt = AV_PIX_FMT_YUV420P;
		(*cc)->pix_fmt = AV_PIX_FMT_YUV420P;
		(*cc)->gop_size      = 12; /* emit one intra frame every twelve frames at most */
//		(*cc)->pix_fmt       = STREAM_PIX_FMT;
		(*cc)->width = 1920;
		(*cc)->height = 1080;


#if 0
//		(*cc)->width = first_frame->width;
//		(*cc)->height = first_frame->height;
		(*cc)->field_order = AV_FIELD_PROGRESSIVE;
		(*cc)->color_range = AVCOL_RANGE_MPEG;//first_frame->color_range;
		(*cc)->color_primaries = AVCOL_PRI_BT709;//first_frame->color_primaries;
		(*cc)->color_trc = AVCOL_TRC_BT709;//first_frame->color_trc;
		(*cc)->colorspace = AVCOL_SPC_BT709;//first_frame->colorspace;
		(*cc)->chroma_sample_location = AVCHROMA_LOC_LEFT;//first_frame->chroma_location;
		(*cc)->sample_aspect_ratio = {1, 1};//first_frame->sample_aspect_ratio;
#endif


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
	st->codecpar->frame_size = (*cc)->frame_size;
	st->time_base = (*cc)->time_base;

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

	AVPixelFormat source_pixel_format = AV_PIX_FMT_YUYV422;

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
	SwrContext *swr_ctx = nullptr;
	SwsContext *sws_ctx = nullptr;

	double audio_pts = 0;
	double video_pts = 0;

	AVCodecContext *video_codec_context = nullptr;
	AVCodecContext *audio_codec_context = nullptr;
	AVFrame *video_frame = nullptr;
	MyPicture src_picture;
	MyPicture dst_picture;
	int frame_count = 0;
	AVStream *audio_st = nullptr;
	AVStream *video_st = nullptr;


	QMutex mutex;
	std::deque<Image> video_frames;
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

bool VideoEncoder::get_video_frame(MyPicture *pict, int frame_index, int width, int height)
{
	Image img;
	{
		QMutexLocker lock(&m->mutex);
		if (!m->video_frames.empty()) {
			std::swap(img, m->video_frames.front());
			m->video_frames.pop_front();
		}
	}
	if (img.width() != width || img.height() != height) return false;

	img = img.convertToFormat(Image::Format::YUYV8);
	uint8_t *p = pict->pointers[0];
	memcpy(p, img.bits(), img.bytesPerLine() * height);

	return true;
}

bool VideoEncoder::open_video(AVCodecContext *cc, AVFormatContext *fc, AVCodec const *codec, AVStream *st)
{
	(void)fc;
	int ret;
	AVCodecParameters *c = st->codecpar;

	AVDictionary* codec_options = nullptr;
//	{
//		av_dict_set(&codec_options, "preset", "medium", 0);
//		av_dict_set(&codec_options, "crf", "22", 0);
//		av_dict_set(&codec_options, "profile", "high", 0);
//		av_dict_set(&codec_options, "level", "4.0", 0);
//	}

	/* open the codec */
	ret = avcodec_open2(cc, codec, &codec_options);
	if (ret < 0) {
		//		fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
//		exit(1);
		return false;
	}
	/* allocate and init a re-usable frame */
	m->video_frame = av_frame_alloc();
	if (!m->video_frame) {
		fprintf(stderr, "Could not allocate video frame\n");
//		exit(1);
		return false;
	}
	m->video_frame->format = c->format;
	m->video_frame->width = c->width;
	m->video_frame->height = c->height;
	/* Allocate the encoded raw picture. */
	ret = MyPicture_alloc(&m->dst_picture, c->width, c->height, c->format);
	if (ret < 0) {
		//		fprintf(stderr, "Could not allocate picture: %s\n", av_err2str(ret));
//		exit(1);
		return false;
	}
	ret = MyPicture_alloc(&m->src_picture, c->width, c->height, m->source_pixel_format);
	if (ret < 0) {
		//			fprintf(stderr, "Could not allocate temporary picture: %s\n", av_err2str(ret));
//		exit(1);
		return false;
	}
	/* copy data and linesize picture pointers to frame */
//	*((MyPicture *)m->video_frame) = m->dst_picture;
	for (int i = 0; i < 4; i++) {
		m->video_frame->data[i] = m->dst_picture.pointers[i];
		m->video_frame->linesize[i] = m->dst_picture.linesize[i];
	}

	return true;
}

void VideoEncoder::write_video_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush)
{
	if (!st) return;
	if (!m->video_frame) return;

	int ret;
	AVCodecParameters *c = st->codecpar;
	if (!flush) {
		/* as we only generate a YUV420P picture, we must convert it
		 * to the codec pixel format if needed */
		if (!m->sws_ctx) {
			m->sws_ctx = sws_getContext(c->width, c->height, m->source_pixel_format, c->width, c->height, (AVPixelFormat)c->format, sws_flags, nullptr, nullptr, nullptr);
			if (!m->sws_ctx) {
				fprintf(stderr, "Could not initialize the conversion context\n");
				exit(1);
			}
		}
		if (!get_video_frame(&m->src_picture, m->frame_count, c->width, c->height)) {
			return;
		}
		sws_scale(m->sws_ctx, (const uint8_t * const *)m->src_picture.pointers, m->src_picture.linesize, 0, c->height, m->dst_picture.pointers, m->dst_picture.linesize);
	}
//	if ((oc->oformat->flags & AVFMT_RAWPICTURE) && !flush) {
//		/* Raw video case - directly store the picture in the packet */
//		AVPacket pkt;
//		av_init_packet(&pkt);
//		pkt.flags        |= AV_PKT_FLAG_KEY;
//		pkt.stream_index  = st->index;
//		pkt.data          = m->dst_picture.data[0];
//		pkt.size          = sizeof(MyPicture);
//		ret = av_interleaved_write_frame(oc, &pkt);
//	} else
	{
		AVPacket pkt = {};
		int got_packet;
		av_init_packet(&pkt);
		/* encode the image */
		m->video_frame->pts = m->frame_count;
#if 0
		ret = avcodec_encode_video2(cc, &pkt, flush ? nullptr : m->frame, &got_packet);
		if (ret < 0) {
			//			fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
			exit(1);
		}
#else
		got_packet = 0;
//		m->audio_frame->key_frame = 0;
//		m->audio_frame->pict_type = AV_PICTURE_TYPE_NONE;
		ret = avcodec_send_frame(cc, flush ? nullptr : m->video_frame);
		if (ret < 0 && ret != AVERROR_EOF) {
			fprintf(stderr, "error during sending frame");
			return;
		}
//		ret = avcodec_receive_packet(cc, &pkt);
//		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return;

//		if (ret < 0) {
//			fprintf(stderr, "Error during receiving frame, error");
//			av_packet_unref(&pkt);
//			return;
//		}
		while (avcodec_receive_packet(cc, &pkt) == 0) {
//			pkt.stream_index = 0;
			ret = write_frame(cc, fc, &st->time_base, st, &pkt);
		}
//		got_packet = 1;

#endif
		/* If size is zero, it means the image was buffered. */
//		if (got_packet) {
//			ret = write_frame(fc, &st->time_base, st, &pkt);
//		} else {
			if (flush) {
				m->video_is_eof = true;
			}
//			ret = 0;
//		}
	}
	if (ret < 0) {
		//		fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
		exit(1);
	}
	m->video_pts = m->video_frame->pts;
	m->frame_count++;
}

void VideoEncoder::close_video(AVFormatContext *fc, AVStream *st)
{
	(void)fc;
	if (m->video_codec_context) {
		avcodec_free_context(&m->video_codec_context);
	}
	MyPicture_free(&m->src_picture);
	MyPicture_free(&m->dst_picture);
	av_frame_free(&m->video_frame);
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

bool VideoEncoder::open_audio(AVCodecContext *cc, AVFormatContext *fc, AVCodec const *codec, AVStream *st)
{
	int ret;
	AVCodecParameters *cp = st->codecpar;
	/* allocate and init a re-usable frame */
	m->audio_frame = av_frame_alloc();
	if (!m->audio_frame) {
		fprintf(stderr, "Could not allocate audio frame\n");
		return false;
	}
	m->audio_frame->format = st->codecpar->format;
	m->audio_frame->channels = st->codecpar->channels;
	/* open it */
//	c->strict_std_compliance = oc->strict_std_compliance;
	ret = avcodec_open2(cc, codec, nullptr);
	if (ret < 0) {
		//		fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
		return false;
	}
	/* init signal generator */
	m->t     = 0;
	m->tincr = 2 * M_PI * 110.0 / cp->sample_rate;
	/* increment frequency by 110 Hz per second */
	m->tincr2 = 2 * M_PI * 110.0 / cp->sample_rate / cp->sample_rate;
	m->src_nb_samples = cc->frame_size;//cp->frame_size;
	ret = av_samples_alloc_array_and_samples(&m->src_samples_data, &m->src_samples_linesize, cp->channels, m->src_nb_samples, AV_SAMPLE_FMT_S16, 0);
	if (ret < 0) {
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
		if ((ret = swr_init(m->swr_ctx)) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			return false;
		}
		ret = av_samples_alloc_array_and_samples(&m->dst_samples_data, &m->dst_samples_linesize, cp->channels, m->max_dst_nb_samples, (AVSampleFormat)cp->format, 0);
		if (ret < 0) {
			fprintf(stderr, "Could not allocate destination samples\n");
			return false;
		}
	} else {
		m->dst_samples_data = m->src_samples_data;
	}
	m->dst_samples_size = av_samples_get_buffer_size(nullptr, cp->channels, m->max_dst_nb_samples, (AVSampleFormat)cp->format, 0);
	return true;
}

void VideoEncoder::write_audio_frame(AVCodecContext *cc, AVFormatContext *fc, AVStream *st, bool flush)
{
	if (!st) return;
	if (!m->audio_frame) return;

	AVPacket pkt = {}; // data and size must be 0;
	int got_packet, ret, dst_nb_samples;
	av_init_packet(&pkt);
	AVCodecParameters *cp = st->codecpar;
	if (!flush) {
		if (!get_audio_frame((int16_t *)m->src_samples_data[0], m->src_nb_samples, cp->channels)) {
			return;
		}
		/* convert samples from native format to destination codec format, using the resampler */
		if (m->swr_ctx) {
			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(m->swr_ctx, cp->sample_rate) + m->src_nb_samples, cp->sample_rate, cp->sample_rate, AV_ROUND_UP);
			if (dst_nb_samples > m->max_dst_nb_samples) {
				av_free(m->dst_samples_data[0]);
				ret = av_samples_alloc(m->dst_samples_data, &m->dst_samples_linesize, cp->channels, dst_nb_samples, (AVSampleFormat)cp->format, 0);
				if (ret < 0) {
					return;
				}
				m->max_dst_nb_samples = dst_nb_samples;
				m->dst_samples_size = av_samples_get_buffer_size(nullptr, cp->channels, dst_nb_samples, (AVSampleFormat)cp->format, 0);
			}
			/* convert to destination format */
			ret = swr_convert(m->swr_ctx, m->dst_samples_data, dst_nb_samples, (const uint8_t **)m->src_samples_data, m->src_nb_samples);
			if (ret < 0) {
				fprintf(stderr, "Error while converting\n");
				return;
			}
			dst_nb_samples = ret;
		} else {
			dst_nb_samples = m->src_nb_samples;
		}
		m->audio_frame->nb_samples = dst_nb_samples;
		AVRational rate = { 1, cp->sample_rate };
		m->audio_frame->pts = av_rescale_q(m->samples_count, rate, st->time_base);
		ret = avcodec_fill_audio_frame(m->audio_frame, cp->channels, (AVSampleFormat)cp->format, m->dst_samples_data[0], m->dst_samples_size, 0);
		m->samples_count += dst_nb_samples;
	}
#if 0
	ret = avcodec_encode_audio2(&cc, &pkt, flush ? nullptr : m->audio_frame, &got_packet);
	if (ret < 0) {
		//		fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
		exit(1);
	}
#else
	got_packet = 0;
	AVFrame *f = m->audio_frame;
	ret = avcodec_send_frame(cc, flush ? nullptr : m->audio_frame);
	if (ret < 0 && ret != AVERROR_EOF) {
		fprintf(stderr, "error during sending frame");
		return;
	}
//	ret = avcodec_receive_packet(cc, &pkt);
//	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return;

//	if (ret < 0) {
//		fprintf(stderr, "Error during receiving frame, error");
//		av_packet_unref(&pkt);
//		return;
//	}
	while (avcodec_receive_packet(cc, &pkt) == 0) {
//		pkt.stream_index = 0;
//		av_packet_rescale_ts(&pkt, cc->time_base, st->time_base);
		ret = write_frame(cc, fc, &st->time_base, st, &pkt);
		if (ret != 0) {
			printf("av_write_frame failed\n");
		}
	}
	got_packet = 1;
#endif
//	if (!got_packet) {
		if (flush) {
			m->audio_is_eof = true;
		}
//		return;
//	}
//	ret = write_frame(fc, &st->time_base, st, &pkt);
//	if (ret < 0) {
//		//		fprintf(stderr, "Error while writing audio frame: %s\n", av_err2str(ret));
//		return;
//	}
	m->audio_pts = (double)m->samples_count * st->time_base.den / st->time_base.num / cp->sample_rate;
}

void VideoEncoder::close_audio(AVFormatContext *fc, AVStream *st)
{
	(void)fc;
	if (m->audio_codec_context) {
		avcodec_free_context(&m->audio_codec_context);
	}
//	avcodec_close(st->codec);
	if (m->dst_samples_data != m->src_samples_data) {
		av_free(m->dst_samples_data[0]);
		av_free(m->dst_samples_data);
	}
	av_free(m->src_samples_data[0]);
	av_free(m->src_samples_data);
	av_frame_free(&m->audio_frame);
}

bool VideoEncoder::is_recording() const
{
	return (m->video_st && !m->video_is_eof) || (m->audio_st && !m->audio_is_eof);
}

int VideoEncoder::save()
{
	std::string filename = m->filepath.toStdString();
	AVOutputFormat const *fmt;
	AVFormatContext *fc;
	AVCodec const *audio_codec;
	AVCodec const *video_codec;
	double audio_time, video_time;
	int ret;

	//	av_log_set_level(AV_LOG_ERROR);
	av_log_set_level(AV_LOG_WARNING);

	/* Initialize libavcodec, and register all codecs and formats. */
//	av_register_all();
	/* allocate the output media context */
	std::string ext;
	{
		auto pos = filename.find_last_of('.');
		if (pos != std::string::npos) {
			ext = filename.substr(pos + 1);
		}
	}
	AVOutputFormat oformat = *av_guess_format("avi", filename.c_str(), nullptr);
	oformat.flags |= AVFMT_TS_NONSTRICT;
	/* open the output file, if needed */
	AVIOContext *io_context = nullptr;
	if (!(oformat.flags & AVFMT_NOFILE)) {
		ret = avio_open(&io_context, filename.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			//			fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
			return 1;
		}
	}
	ret = avformat_alloc_output_context2(&fc, &oformat, nullptr, nullptr);
	if (!fc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		ret = avformat_alloc_output_context2(&fc, nullptr, "avi", filename.c_str());
		return 1;
	}
	fc->pb = io_context;
//	fc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	fmt = fc->oformat;
//	assert(fmt->audio_codec == AV_CODEC_ID_MP3);
//	assert(fmt->video_codec == AV_CODEC_ID_MPEG4);
//	av_dump_format(fc, 0, filename.c_str(), 1);
	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	m->video_st = nullptr;
	m->audio_st = nullptr;
	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		m->video_st = add_stream(fc, &m->video_codec_context, &video_codec, fmt->video_codec, m->aopt, m->vopt);
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		m->audio_st = add_stream(fc, &m->audio_codec_context, &audio_codec, fmt->audio_codec, m->aopt, m->vopt);
	}
	if (avformat_write_header(fc, nullptr) < 0) {
		fprintf(stderr, "avformat_write_header failed\n");
	}
	/* Now that all the parameters are set, we can open the audio and
		 * video codecs and allocate the necessary encode buffers. */
	if (m->video_st) {
		if (!open_video(m->video_codec_context, fc, video_codec, m->video_st)) {
			return -1;
		}
	}
	if (m->audio_st) {
		if (!open_audio(m->audio_codec_context, fc, audio_codec, m->audio_st)) {
			return -1;
		}
	}
	/* Write the stream header, if any. */
	bool flush = false;
	while (is_recording()) {
		if (isInterruptionRequested()) {
			flush = true;
			break;
		}
		/* Compute current audio and video time. */
		video_time = (m->video_st && !m->video_is_eof) ? m->video_pts * av_q2d(m->video_st->time_base) : INFINITY;
		audio_time = (m->audio_st && !m->audio_is_eof) ? m->audio_pts * av_q2d(m->audio_st->time_base) : INFINITY;
		if (m->audio_st && !m->audio_is_eof && audio_time <= video_time) {
			write_audio_frame(m->audio_codec_context, fc, m->audio_st, flush);
		} else if (m->video_st && !m->video_is_eof && video_time < audio_time) {
			write_video_frame(m->video_codec_context, fc, m->video_st, flush);
		}
	}
	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	av_write_trailer(fc);
	/* Close each codec. */
	if (m->video_st) {
		close_video(fc, m->video_st);
		m->video_st = nullptr;
	}
	if (m->audio_st) {
		close_audio(fc, m->audio_st);
		m->audio_st = nullptr;
	}
	if (!(fmt->flags & AVFMT_NOFILE)) {
		/* Close the output file. */
		avio_close(fc->pb);
	}

	/* free the stream */
	avformat_free_context(fc);
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

bool VideoEncoder::putVideoFrame(const Image &img)
{
	if (img) {
		QMutexLocker lock(&m->mutex);
		if (is_recording()) {
			m->video_frames.push_back(img);
			while (m->video_frames.size() > 100) {
				m->video_frames.pop_front();
			}
			return true;
		}
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

