#include "DeckLinkAPI.h"

HRESULT GetDeckLinkIterator(IDeckLinkIterator **deckLinkIterator)
{
	HRESULT result = S_OK;

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
#ifdef Q_OS_WIN
	if (CoCreateInstance(CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL, IID_IDeckLinkIterator, (void **)deckLinkIterator) != S_OK) {
		*deckLinkIterator = nullptr;
		result = E_FAIL;
	}
#else
	*deckLinkIterator = CreateDeckLinkIteratorInstance();
	if (!*deckLinkIterator) {
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		result = E_FAIL;
	}
#endif

	return result;
}

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
