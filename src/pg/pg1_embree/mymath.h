#ifndef MY_MATH_H_
#define MY_MATH_H_

#define _USE_MATH_DEFINES
#include <math.h>
#include <float.h>
#include "structs.h"

#define M_1_2PI     0.15915494309189533576888376337251  // 1/2pi
#define M_1_4PI		0.07957747154594766788444188168626	// 1/4pi
#define M_2PI		6.283185307179586476925286766559	// 2pi
#define M_4PI		12.566370614359172953850573533118	// 4pi

template <class T> inline T sqr( const T x )
{
	return x * x;
}

inline Normal3f normalize( const Normal3f & n )
{
	float tmp = sqr( n.x ) + sqr( n.y ) + sqr( n.z );

	if ( fabsf( tmp ) > FLT_EPSILON )
	{
		tmp = 1.0f / tmp;
		return Normal3f{ n.x * tmp, n.y * tmp, n.z * tmp };
	}

	return n;
}

inline float deg2rad( const float x )
{
	return x * float( M_PI ) / 180.0f;
}

inline int fact(const int x)
{
	int val = 1;
	for (int i = x; i > 0; i--)
		val *= i;
	return val;
}

#endif
