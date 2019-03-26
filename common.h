#ifndef COMMON_H
#define COMMON_H

// Define custom event type
static const QEvent::Type kAddDeviceEvent			= static_cast<QEvent::Type>(QEvent::User + 1);
static const QEvent::Type kRemoveDeviceEvent		= static_cast<QEvent::Type>(QEvent::User + 2);
static const QEvent::Type kVideoFormatChangedEvent	= static_cast<QEvent::Type>(QEvent::User + 3);
static const QEvent::Type kVideoFrameArrivedEvent	= static_cast<QEvent::Type>(QEvent::User + 4);
static const QEvent::Type kProfileActivatedEvent	= static_cast<QEvent::Type>(QEvent::User + 5);

#endif // COMMON_H
