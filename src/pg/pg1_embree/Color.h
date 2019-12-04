#pragma once
#include "vector3.h"

typedef struct Color {
	Vector3 RGB;
	Vector3 Emission;

	Color() {
		RGB = { 0,0,0 };
		Emission = { 0,0,0 };
	};

	Color(Vector3 rgb, Vector3 emission) {
		RGB = rgb;
		Emission = emission;
	}
	
	Color(const float* rgb, const float* emission)
	{
		assert(rgb != NULL);

		RGB = Vector3(rgb);
		Emission = emission == NULL ? Vector3(0, 0, 0) : Vector3(emission);
	}



	friend Color operator-(const Color& v);


	friend Color operator+(const Color& u, const float& v);
	friend Color operator-(const Color& u, const float& v);
	friend Color operator+(const Color& u, const Color& v);
	friend Color operator-(const Color& u, const Color& v);

	friend Color operator*(const Color& v, const float a);
	friend Color operator*(const float a, const Color& v);
	friend Color operator*(const Color& u, const Color& v);

	friend Color operator/(const Color& v, const float a);

	friend void operator+=(Color& u, const Color& v);
	friend void operator-=(Color& u, const Color& v);
	friend void operator*=(Color& v, const float a);
	friend void operator/=(Color& v, const float a);
};

#define Color_Empty Color{{0,0,0},{0,0,0}}