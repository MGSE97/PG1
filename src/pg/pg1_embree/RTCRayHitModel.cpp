#include "stdafx.h"
#include "RTCRayHitModel.h"

RTCRayHitModel::RTCRayHitModel()
= default;


RTCRayHitModel::RTCRayHitModel(const RTCRayHit& ray_hit, const RTCScene* ray_scene, const float& current_ior)
{
	n1 = current_ior;
	core = ray_hit;
	scene = ray_scene;
	from = Vector3(ray_hit.ray.org_x, ray_hit.ray.org_y, ray_hit.ray.org_z);
	dir = Vector3(ray_hit.ray.dir_x, ray_hit.ray.dir_y, ray_hit.ray.dir_z);
	dir.Normalize();
	hit = from + ray_hit.ray.tfar * Vector3(ray_hit.ray.dir_x, ray_hit.ray.dir_y, ray_hit.ray.dir_z);
	if(core.hit.geomID != RTC_INVALID_GEOMETRY_ID)
		load_geometry_data();
}

void RTCRayHitModel::load_geometry_data()
{
	// we hit something
	geometry = rtcGetGeometry(*scene, core.hit.geomID);

	// get interpolated normal
	rtcInterpolate0(geometry, core.hit.primID, core.hit.u, core.hit.v, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, &raw_normal.x, 3);
	normal = { raw_normal.x, raw_normal.y, raw_normal.z };
	if (normal.DotProduct(-dir) < 0)
		normal = -normal;

	rtcInterpolate0(geometry, core.hit.primID, core.hit.u, core.hit.v, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 1, &tex_coord.u, 2);

	material = static_cast<Material*>(rtcGetGeometryUserData(geometry));

	n2 = n1 > IOR_AIR ? IOR_AIR : material->ior;

	load_material();

	rouletteRho = colorDiffuse.LargestValue();
}

void RTCRayHitModel::load_material()
{
	// Get Difuse
	if (material != nullptr)
	{
		colorDiffuse = material->diffuse;
		Texture* diffuse = material->get_texture(material->kDiffuseMapSlot);
		if (diffuse != nullptr)
		{
			Color3f texlet = diffuse->get_texel(tex_coord.u, 1.0f - tex_coord.v);
			colorDiffuse.x = texlet.r;
			colorDiffuse.y = texlet.g;
			colorDiffuse.z = texlet.b;
		}

		colorSpecular = material->specular;
	}
}

void RTCRayHitModel::calc_reflection()
{
	reflected = (-dir).Reflect(normal);
	//reflected.Normalize();
}

void RTCRayHitModel::calc_refraction()
{
	const float dirNormal = dir.DotProduct(normal), n12 = n1 / n2;
	refracted = n12 * dir - (n12 * dirNormal + sqrt(1.f - n12 * n12 * (1.f - dirNormal * dirNormal))) * normal;
	//refracted.Normalize();
}

void RTCRayHitModel::calc_fresnel()
{
	calc_reflection();
	calc_refraction();
	if (material->isMirror())
		R = 1;
	else
	{

		float
			o = n1 <= n2 ? dir.DotProduct(normal) : refracted.DotProduct(-normal),
			cosi = cosf(o),
			sini = 1.f - cosi;
		// Total internal reflection
		if (sini >= 1) {
			R = 1;
		}
		else {
			// Compute fresnel
			float
				n12 = (n1 - n2) / (n1 + n2),
				R0 = n12 * n12;
			R = max(R0 + (1.f - R0) * sini * sini * sini * sini * sini, 0);

			/*float o1 = dir.DotProduct(normal), o2 = refracted.DotProduct(-normal),
				co1 = cos(o1), co2 = cos(o2),
				n2o2 = n2 * co2, n1o1 = n1 * co1,
				n2o1 = n2 * co1, n1o2 = n1 * co2,
				Rs = (n2o2 - n1o1) / (n2o2 + n1o1),
				Rp = (n2o1 - n1o2) / (n2o1 + n1o2);
			R = (Rs*Rs + Rp*Rp) / 2.f;*/
		}
	}
}

Color RTCRayHitModel::calc_result_color(const float& distance)
{
	Color color = (colorRefracted * (1.f - R) + colorReflected * R);
	if (distance >= 0)
	{
		Vector3 exp = material->emission.Exp(-distance);
		color = color * Color{ exp, exp };
	}
		//color = color * material->attenuation.Exp(-distance);
	return color;
}
