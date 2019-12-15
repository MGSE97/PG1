#pragma once
#include <embree3\rtcore_ray.h>
#include "vector3.h"

class Sample
{
public:
	Sample() {};
	RTCRayHit Ray;
	RTCRayHitModel Model;
	float PDF;
	Vector3 Dir;
	Vector3 OmegaI;
	float OmegaIN;
	bool colision;
};