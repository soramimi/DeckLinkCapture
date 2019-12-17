
#include "DeckLinkAPI.h"

// DLString

DLString::~DLString()
{
	clear();
}

void DLString::clear()
{
	if (str) {
#ifdef Q_OS_WIN
		SysFreeString(str);
#endif
#ifdef Q_OS_MAC
		CFRelease(str);
#endif
#ifdef Q_OS_LINUX
		free((void *)str);
#endif
		str = nullptr;
	}
}
