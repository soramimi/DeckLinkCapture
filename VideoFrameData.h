#ifndef VIDEOFRAMEDATA_H
#define VIDEOFRAMEDATA_H

#include "Image.h"
#include <QImage>
#include <QMetaType>
#include <memory>

#include "AncillaryDataTable.h"
#include "DeckLinkAPI.h"

class VideoFrameData {
public:
	enum State {
		Idle,
		Busy,
		Ready,
	};
	struct Data {
		State state = Idle;
		Image image;
		QByteArray audio;
		QImage image_for_view;
		bool signal_valid = false;

		AncillaryDataStruct ancillary_data = {};
		BMDPixelFormat pixfmt = bmdFormatUnspecified;
		HDRMetadataStruct hdr_metadata = {};
	};
	std::shared_ptr<Data> d;
	VideoFrameData()
		: d(std::make_shared<Data>())
	{
	}
	operator bool () const
	{
		return d->signal_valid && d->image;
	}
	int width() const
	{
		return d->image.width();
	}
	int height() const
	{
		return d->image.height();
	}
};

Q_DECLARE_METATYPE(VideoFrameData)

#endif // VIDEOFRAMEDATA_H
