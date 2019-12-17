#ifndef DECKLINKAPI_H
#define DECKLINKAPI_H

#include <QMetaType>
#include <QString>

#if defined(Q_OS_WIN)
#include "sdk/Win/DeckLinkAPI_h.h"
typedef IID CFUUIDBytes;

class DLString {
private:
	BSTR str = nullptr;
public:
	~DLString();
	void clear();
	BSTR *operator & ()
	{
		return &str;
	}
	operator QString ()
	{
		return str ? QString::fromUtf16((ushort const *)str) : QString();
	}
	operator std::string ()
	{
		return operator QString ().toStdString();
	}
	bool empty() const
	{
		return !(str && *str);
	}
};

#elif defined(Q_OS_MAC)
#include "sdk/Mac/include/DeckLinkAPI.h"

typedef bool BOOL;

class DLString {
private:
	CFStringRef str = nullptr;
public:
	~DLString();
	void clear();
	CFStringRef *operator & ()
	{
		return &str;
	}
	operator QString () const
	{
		if (!str) return QString();
		CFIndex length = CFStringGetLength(str);
		if (length == 0) return QString();
		QString string(length, Qt::Uninitialized);
		CFStringGetCharacters(str, CFRangeMake(0, length), reinterpret_cast<UniChar *>(const_cast<QChar *>(string.unicode())));
		return string;
	}
	operator std::string () const
	{
		return operator QString ().toStdString();
	}
	bool empty() const
	{
		return !(str && CFStringGetLength(str) > 0);
	}
};

#elif defined(Q_OS_LINUX)
#include "sdk/Linux/include/DeckLinkAPI.h"
typedef bool BOOL;

class DLString {
private:
	char const *str = nullptr;
public:
	~DLString();
	void clear();
	char const **operator & ()
	{
		return &str;
	}
	operator QString () const
	{
		return str ? QString::fromUtf8(str) : QString();
	}
	operator std::string () const
	{
		return str ? std::string(str) : std::string();
	}
	bool empty() const
	{
		return !(str && *str);
	}
};

#endif

HRESULT GetDeckLinkIterator(IDeckLinkIterator **deckLinkIterator);

#endif // DECKLINKAPI_H
