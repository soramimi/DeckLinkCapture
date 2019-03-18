#ifndef DECKLINKAPI_H
#define DECKLINKAPI_H

#include <QMetaType>

#ifdef Q_OS_WIN
#include "sdk/Win/DeckLinkAPI_h.h"
typedef IID CFUUIDBytes;
#else
#include "sdk/Linux/include/DeckLinkAPI.h"
typedef bool BOOL;
typedef char *BSTR;
#endif

#endif // DECKLINKAPI_H
