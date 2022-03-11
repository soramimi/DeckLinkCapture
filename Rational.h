#ifndef RATIONAL_H
#define RATIONAL_H

#include <cstdint>
#include <QMetaType>

class Rational {
public:
	int64_t num = 0;
	int64_t den = 1;
	Rational() = default;
	Rational(int64_t num, int64_t den)
		: num(num)
		, den(den)
	{
	}
};

Q_DECLARE_METATYPE(Rational)

#endif // RATIONAL_H
