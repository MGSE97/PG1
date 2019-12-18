#pragma once
#include "vector3.h"
#include "structs.h"
#include "material.h"
#include "simpleguidx11.h"

class RTCRayHitModel
{
public:
	RTCRayHitModel();

	RTCRayHitModel(const RTCRayHit& ray_hit, const RTCScene* ray_scene, const float& current_ior);
	void load_geometry_data();
	
	void calc_reflection();
	void calc_refraction();
	void calc_fresnel();
	void load_material();

	Vector3 calc_result_color(const float& distance);

	RTCRayHit core{};
	const RTCScene* scene{};
	RTCGeometry geometry{};
	Vector3 from;
	Vector3 dir;
	Vector3 hit;
	Vector3 normal;
	Normal3f raw_normal{};
	Coord2f tex_coord{};
	Material* material{};

	float R;
	Vector3 reflected;
	Vector3 refracted;
	float n1;
	float n2;

	Vector3 colorRefracted;
	Vector3 colorReflected;

	Vector3 colorDiffuse;
	Vector3 colorSpecular;
	float rouletteRho;
	bool roulette;
};
