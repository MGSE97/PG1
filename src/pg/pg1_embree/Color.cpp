#include "stdafx.h"
#include "Color.h"

// --- operátory ------

Color operator-(const Color& v)
{
	return { -v.RGB, -v.Emission };
}

Color operator+(const Color& u, const float& v)
{
	return { u.RGB + v, u.Emission + v};
}

Color operator+(const Color& u, const Color& v)
{
	return { u.RGB + v.RGB, u.Emission + v.Emission };
}

Color operator-(const Color& u, const float& v)
{
	return { u.RGB - v, u.Emission - v };
}

Color operator-(const Color& u, const Color& v)
{
	return { u.RGB - v.RGB, u.Emission - v.Emission };
}

Color operator*(const Color& v, const float a)
{
	return { v.RGB*a, v.Emission*a };
}

Color operator*(const float a, const Color& v)
{
	return { v.RGB*a, v.Emission*a };
}

Color operator*(const Color& u, const Color& v)
{
	return { u.RGB * v.RGB, u.Emission * v.Emission };
}

Color operator/(const Color& v, const float a)
{
	return v * (1 / a);
}

void operator+=(Color& u, const Color& v)
{
	u.RGB += v.RGB;
	u.Emission += v.Emission;
}

void operator-=(Color& u, const Color& v)
{
	u.RGB -= v.RGB;
	u.Emission -= v.Emission;
}

void operator*=(Color& v, const float a)
{
	v.RGB *= a;
	v.Emission *= a;
}

void operator/=(Color& v, const float a)
{
	const float r = 1 / a;

	v.RGB *= r;
	v.Emission *= r;
}
