/* 
 * sRGB transform (C++)
 * 
 * Copyright (c) 2017 Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/srgb-transform-library
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

#include "stdafx.h"
#include "SrgbTransform.h"

namespace SrgbTransform {

	/*---- sRGB values to linear intensities ----*/

	float srgbToLinear(float x) {
		if (x <= 0.0f)
			return 0.0f;
		else if (x >= 1.0f)
			return 1.0f;
		else if (x < 0.04045f)
			return x / 12.92f;
		else
			return powf((x + 0.055f) / 1.055f, 2.4f);
	}


	double srgbToLinear(double x) {
		if (x <= 0.0)
			return 0.0;
		else if (x >= 1.0)
			return 1.0;
		else if (x < 0.04045)
			return x / 12.92;
		else
			return powf((x + 0.055) / 1.055, 2.4);
	}

	Vector3 srgbToLinear(Vector3 srgb)
	{
		return Vector3(srgbToLinear(srgb.x), srgbToLinear(srgb.y), srgbToLinear(srgb.z));
	}

	/*---- Linear intensities to sRGB values ----*/

	float linearToSrgb(float x) {
		if (x <= 0.0f)
			return 0.0f;
		else if (x >= 1.0f)
			return 1.0f;
		else if (x < 0.0031308f)
			return x * 12.92f;
		else
			return powf(x, 1.0f / 2.4f) * 1.055f - 0.055f;
	}


	double linearToSrgb(double x) {
		if (x <= 0.0)
			return 0.0;
		else if (x >= 1.0)
			return 1.0;
		else if (x < 0.0031308)
			return x * 12.92;
		else
			return powf(x, 1.0 / 2.4) * 1.055 - 0.055;
	}

	Vector3 linearToSrgb(Vector3 linear)
	{
		return Vector3(linearToSrgb(linear.x), linearToSrgb(linear.y), linearToSrgb(linear.z));
	}
}
