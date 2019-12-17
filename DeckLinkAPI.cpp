
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
#else
		free((void *)str);
#endif
		str = nullptr;
	}
}
