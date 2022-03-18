#ifndef CAPTUREFRAME_H
#define CAPTUREFRAME_H

#include "Image.h"
#include <QImage>
#include <QMetaType>
#include "AncillaryDataTable.h"

class CaptureFrame {
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
		AncillaryDataStruct ancillary_data = {};
		HDRMetadataStruct hdr_metadata = {};
		bool signal_valid = false;
	};
	std::shared_ptr<Data> d;
	CaptureFrame()
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

Q_DECLARE_METATYPE(CaptureFrame)

#endif // CAPTUREFRAME_H
