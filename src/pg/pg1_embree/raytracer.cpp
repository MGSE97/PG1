#include "stdafx.h"
#include "raytracer.h"
#include "objloader.h"
#include "tutorials.h"
#include <math.h>
#include "SrgbTransform.h"
#include <chrono>
#include <iostream>
#include <float.h>
#include "RTCRayHitModel.h"
#include "mymath.h"

chrono::time_point<chrono::steady_clock> Raytracer::begin()
{
	return chrono::high_resolution_clock::now();
}

void Raytracer::log(chrono::time_point<chrono::steady_clock>& begin, string prefix)
{
	if (debug_)
		#pragma omp atomic
		times[prefix] += std::chrono::duration_cast<std::chrono::milliseconds>(chrono::high_resolution_clock::now() - begin).count();
}

void Raytracer::log(chrono::time_point<chrono::steady_clock>& begin, string prefix, int bump)
{
	if (debug_)
		#pragma omp atomic
		times[prefix.append(" " + std::to_string(bump))] += std::chrono::duration_cast<std::chrono::milliseconds>(chrono::high_resolution_clock::now() - begin).count();
}


Raytracer::Raytracer(const int width, const int height,
	const float fov_y, const Vector3 view_from, const Vector3 view_at,
	Vector3* light, Vector3* lightPower,
	const Vector3* background,
	const char * config ) : SimpleGuiDX11( width, height )
{
	InitDeviceAndScene( config );

	camera_ = Camera( width, height, fov_y, view_from, view_at );
	
	if (background == nullptr)
	{
		cubeMap_ = new CubeMap("../../../data/sky185/sky185rt.png",
			"../../../data/sky185/sky185lf.png", "../../../data/sky185/sky185bk.png",
			"../../../data/sky185/sky185ft.png", "../../../data/sky185/sky185up.png",
			"../../../data/sky185/sky185dn.png");
	}
	else
		cubeMap_ = new CubeMap("../../../data/sky185/sky185rt.png",
			"../../../data/sky185/sky185lf.png", "../../../data/sky185/sky185bk.png",
			"../../../data/sky185/sky185ft.png", "../../../data/sky185/sky185up.png",
			"../../../data/sky185/sky185dn.png", *background);

	light_ = *light;
	lightPower_ = *lightPower;

	ss_e2_ = std::mt19937(rd_());
	ss_dist_ = std::uniform_real_distribution<>(-SS_D, SS_D);

	e2_ = std::mt19937(rd_());
	dist_ = std::uniform_real_distribution<>(-1.f, 1.f);

	times["get_pixel"] = 0;
	for (int i = 0; i < 11; i++)
	{
		times["reflection " + std::to_string(i)] = 0;
		times["refraction " + std::to_string(i)] = 0;
	}
}

Raytracer::~Raytracer()
{
	ReleaseDeviceAndScene();
}

int Raytracer::InitDeviceAndScene( const char * config )
{
	device_ = rtcNewDevice( config );
	error_handler( nullptr, rtcGetDeviceError( device_ ), "Unable to create a new device.\n" );
	rtcSetDeviceErrorFunction( device_, error_handler, nullptr );

	ssize_t triangle_supported = rtcGetDeviceProperty( device_, RTC_DEVICE_PROPERTY_TRIANGLE_GEOMETRY_SUPPORTED );

	// create a new scene bound to the specified device
	scene_ = rtcNewScene( device_ );

	//rtcSetSceneFlags(scene_, RTC_SCENE_FLAG_ROBUST);

	//light_ = Vector3{ 200,300,400 };
	//light_.Normalize();
	//lightPower_ = Vector3{ 1.0f, 1.0f, 1.0f };

	return S_OK;
}

int Raytracer::ReleaseDeviceAndScene()
{
	rtcReleaseScene( scene_ );
	rtcReleaseDevice( device_ );

	return S_OK;
}

void Raytracer::LoadScene( const std::string file_name )
{
	const int no_surfaces = LoadOBJ( file_name.c_str(), surfaces_, materials_ );

	// surfaces loop
	for ( auto surface : surfaces_ )
	{
		RTCGeometry mesh = rtcNewGeometry( device_, RTC_GEOMETRY_TYPE_TRIANGLE );

		Vertex3f * vertices = ( Vertex3f * )rtcSetNewGeometryBuffer(
			mesh, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
			sizeof( Vertex3f ), 3 * surface->no_triangles() );

		Triangle3ui * triangles = ( Triangle3ui * )rtcSetNewGeometryBuffer(
			mesh, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
			sizeof( Triangle3ui ), surface->no_triangles() );

		rtcSetGeometryUserData( mesh, ( void* )( surface->get_material() ) );

		rtcSetGeometryVertexAttributeCount( mesh, 2 );

		Normal3f * normals = ( Normal3f * )rtcSetNewGeometryBuffer(
			mesh, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, RTC_FORMAT_FLOAT3,
			sizeof( Normal3f ), 3 * surface->no_triangles() );

		Coord2f * tex_coords = ( Coord2f * )rtcSetNewGeometryBuffer(
			mesh, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 1, RTC_FORMAT_FLOAT2,
			sizeof( Coord2f ), 3 * surface->no_triangles() );		

		// triangles loop
		for ( int i = 0, k = 0; i < surface->no_triangles(); ++i )
		{
			Triangle & triangle = surface->get_triangle( i );

			// vertices loop
			for ( int j = 0; j < 3; ++j, ++k )
			{
				const Vertex & vertex = triangle.vertex( j );

				vertices[k].x = vertex.position.x;
				vertices[k].y = vertex.position.y;
				vertices[k].z = vertex.position.z;

				normals[k].x = vertex.normal.x;
				normals[k].y = vertex.normal.y;
				normals[k].z = vertex.normal.z;

				tex_coords[k].u = vertex.texture_coords[0].u;
				tex_coords[k].v = vertex.texture_coords[0].v;
			} // end of vertices loop

			triangles[i].v0 = k - 3;
			triangles[i].v1 = k - 2;
			triangles[i].v2 = k - 1;
		} // end of triangles loop

		rtcCommitGeometry( mesh );
		unsigned int geom_id = rtcAttachGeometry( scene_, mesh );
		rtcReleaseGeometry( mesh );
	} // end of surfaces loop

	rtcCommitScene( scene_ );
}


RTCRayHit Raytracer::prepare_ray_hit(const float t, RTCRay ray, const float& tnear)
{
	ray.tnear = tnear;// FLT_MIN; // start of ray segment
	ray.time = t; // time of this ray for motion blur

	ray.tfar = FLT_MAX; // end of ray segment (set to hit distance)


	// setup a hit
	RTCHit hit;
	hit.geomID = RTC_INVALID_GEOMETRY_ID;
	hit.primID = RTC_INVALID_GEOMETRY_ID;
	hit.Ng_x = 0.0f; // geometry normal
	hit.Ng_y = 0.0f;
	hit.Ng_z = 0.0f;

	RTCRayHit ray_hit;
	ray_hit.ray = ray;
	ray_hit.hit = hit;
	return ray_hit;
}

Vector3 Raytracer::get_material_color(RTCRayHitModel& hit, const float& t)
{
	if (brdf_)
		// (direct + indirect) * albedo / PI
		//return (get_material_phong_color(hit, t) + get_material_brdf_color(hit, t)) / PI;
		return (get_material_phong_color(hit, t) / PI + 2*get_material_brdf_color(hit, t));
		//return get_material_phong_color(hit, t) * get_material_brdf_color(hit, t);
		//return get_material_phong_color(hit, t)*(1.f - hit.R) +get_material_brdf_color(hit, t) * hit.R;
	else
		return get_material_phong_color(hit, t);
}

Vector3 Raytracer::get_material_emission_color(RTCRayHitModel& hit, const float& t)
{
	Vector3 color = Vector3{ 0,0,0};
	if (hit.material != nullptr)
	{
		if (hit.material->emission.data[hit.material->emission.LargestComponent(true)] > 0)
			color = hit.material->ambient + hit.material->diffuse * hit.material->emission;
		else
			color = lightPower_.x * hit.material->ambient;
	}
	return color;
}

Vector3 Raytracer::get_material_phong_color(RTCRayHitModel& hit, const float& t)
{
	Vector3 color = Vector3{ 0.5,0.2,0.55 };
	if(hit.material != nullptr)
	{
		Vector3 lightVec = light_;
		lightVec.Normalize();

		// Check Shadow
		bool shadow = hit.normal.DotProduct(lightVec) < 0;
		if (shadows_)
		{
			auto ray = cast_ray(hit.hit, light_, t);
			if (has_colision(ray))
			{
				auto data = build_ray_model(ray, hit.material->ior);
				while (!has_colision(data))
				{
					ray = cast_ray(hit.hit, light_, t, data.core.ray.tfar + 0.1f);
					if (has_colision(ray))
						data = build_ray_model(ray, hit.material->ior);
					else
						break;
				}

				if (has_colision(data))
					shadow = true;
			}
		}
		
		if (shadow)
		{

			/*if (hit.material->emission.data[hit.material->emission.LargestComponent(true)] > 0)
				color = hit.material->ambient + hit.material->diffuse * hit.material->emission;
			else*/
				color = lightPower_.x * hit.material->ambient;
		}
		else
		{

			// Compute lighting
			Vector3 reflectedVec = lightVec.Reflect(hit.normal);//2 * (light_.CrossProduct(normalVec))*normalVec - light_;
			reflectedVec.Normalize();
			Vector3 cam = hit.from;
			cam.Normalize();

			if (light_map_)
			{
				// Calculate Phong
				color = lightPower_.x * hit.material->ambient +
					lightPower_.y * 1.f * max(cos(hit.normal.DotProduct(lightVec)), 0.f) +
					lightPower_.z * 1.f * powf(max(cos(reflectedVec.DotProduct(cam)), 0.f), hit.material->shininess);
			}
			else
			{
				// Get Difuse
				color = hit.material->diffuse;
				Texture* difuse = hit.material->get_texture(hit.material->kDiffuseMapSlot);
				if (difuse != nullptr)
				{
					Color3f texlet = difuse->get_texel(hit.tex_coord.u, 1.0f - hit.tex_coord.v);
					color.x = texlet.r;
					color.y = texlet.g;
					color.z = texlet.b;
					//color.Normalize();
				}

				Vector3 specular = hit.material->specular;

				// Calculate Phong
				auto power = lightPower_;
				/*if (hit.material->emission.data[hit.material->emission.LargestComponent(true)] > 0)
					power = hit.material->emission;*/
				color = power.x * hit.material->ambient +
					power.y * color * max(hit.normal.DotProduct(lightVec), 0.f) +
					power.z * specular * powf(max(reflectedVec.DotProduct(cam), 0.f), hit.material->shininess);
			}
		}
	}

	return SrgbTransform::srgbToLinear(color);
}

Vector3 Raytracer::get_material_brdf_mirror_color(RTCRayHitModel& hit, const float& t)
{
	auto light = light_;
	light.Normalize();
	return (((-hit.dir).DotProduct(hit.normal) == light.DotProduct(hit.normal) ? Vector3{1, 1, 1} : Vector3{0, 0, 0})) * hit.material->diffuse;
}

Vector3 Raytracer::get_material_brdf_phong_color(RTCRayHitModel& hit, const float& t)
{
	Vector3 color = Vector3{ 0.5,0.2,0.55 };
	if (hit.material != nullptr)
	{
		Vector3 lightVec = light_;
		lightVec.Normalize();

		// Check Shadow
		bool shadow = hit.normal.DotProduct(lightVec) < 0;
		if (shadows_)
		{
			auto ray = cast_ray(hit.hit, light_, t);
			if (has_colision(ray))
			{
				auto data = build_ray_model(ray, hit.material->ior);
				while (!has_colision(data))
				{
					ray = cast_ray(hit.hit, light_, t, data.core.ray.tfar + 0.1f);
					if (has_colision(ray))
						data = build_ray_model(ray, hit.material->ior);
					else
						break;
				}

				if (has_colision(data))
					shadow = true;
			}
		}

		if (shadow)
		{
			if (hit.material->emission.data[hit.material->emission.LargestComponent(true)] > 0)
				color = hit.material->diffuse * hit.material->emission;
			else
				color = { 0, 0, 0 };
		}
		else
		{
			// Compute lighting
			Vector3 lightVec = light_;
			lightVec.Normalize();
			Vector3 reflectedVec = lightVec.Reflect(hit.normal);//2 * (light_.CrossProduct(normalVec))*normalVec - light_;
			reflectedVec.Normalize();
			Vector3 cam = hit.from;
			cam.Normalize();

			if (light_map_)
			{
				color = {1,1,1};
				color =
					hit.material->emission + 
					(lightPower_.y * color * max(hit.normal.DotProduct(lightVec), .0f)) / PI +
					((lightPower_.z * color * powf(max(reflectedVec.DotProduct(cam), .0f), hit.material->shininess)) * (hit.material->shininess + 2.f)) / (2.f * PI) *
					(powf(cos(reflectedVec.DotProduct(hit.normal)), hit.material->shininess) / cos(hit.dir.DotProduct(hit.normal)));
			}
			else
			{
				// Get Difuse
				color = hit.material->diffuse;
				Texture* difuse = hit.material->get_texture(hit.material->kDiffuseMapSlot);
				if (difuse != nullptr)
				{
					Color3f texlet = difuse->get_texel(hit.tex_coord.u, 1.0f - hit.tex_coord.v);
					color.x = texlet.r;
					color.y = texlet.g;
					color.z = texlet.b;
					//color.Normalize();
				}

				Vector3 specular = hit.material->specular;

				//auto power = lightPower_; //get_material_brdf_mirror_color(hit, t);
				// Calculate Phong
				/*color = 
					(color * max(hit.normal.DotProduct(lightVec), .0f)) / PI +
					((specular * powf(max(reflectedVec.DotProduct(cam), .0f), hit.material->shininess)) * (hit.material->shininess + 2.f)) / (2.f * PI) *
					(powf(reflectedVec.DotProduct(hit.normal), hit.material->shininess) / hit.dir.DotProduct(hit.normal));*/
				color = 
					lightPower_.y * color * max(hit.normal.DotProduct(lightVec), 0.f) +
					lightPower_.z * specular * powf(max(reflectedVec.DotProduct(cam), 0.f), hit.material->shininess);
				if (hit.material->emission.data[hit.material->emission.LargestComponent(true)] > 0)
					color = color * hit.material->emission;
			}
		}
	}

	return SrgbTransform::srgbToLinear(color);
}

Vector3 Raytracer::get_material_brdf_ray_color(RTCRayHit& ray, const float& t, const float& ior, int bump)
{
	if (!has_colision(ray))
		return cubeMap_->get_texel(Vector3(ray.ray.dir_x, ray.ray.dir_y, ray.ray.dir_z));

	auto model = build_ray_model(ray, ior);
	/*bump++;
	if (bump <= RAY_MAX_BUMPS)
		return get_material_brdf_color(model, t, bump);
	else*/
		return get_material_brdf_phong_color(model, t);
}

Vector3 Raytracer::get_material_brdf_color(RTCRayHitModel& hit, const float& t, int bump)
{
	if (bump == RAY_MAX_BUMPS)
		return Vector3{ 0,0,0 };
	// Sample half sphere
	int ax_splits = floor(powf(pow(10, BRDF_SAMPLES_EXP), 1.f / 2.f));
	float ax_i = 1.f / (float)ax_splits, pdf = 1 / (2*PI);
	Vector3 color(0, 0, 0);
	for(float u = 0.f; u <= 1.f; u+=ax_i)
		for (float v = 0.f; v <= 1.f; v+=ax_i)
		{
			//float theta = 2*PI * u;
			//float phi = acos(2*v - 1.f);
			/*float theta = PI * u;
			float phi = 2 * v * PI - PI;//acos(2*v - 1.f);
			float x = sin(theta) * cos(phi);
			float y = sin(theta) * sin(phi);
			float z = cos(theta);*/
			float sinTheta = sqrtf(1 - u * u);
			float phi = 2 * M_PI * v;
			float x = sinTheta * cosf(phi);
			float z = sinTheta * sinf(phi);
			auto dir = hit.hit + Vector3(x, u, z);
			/*if (dir.DotProduct(hit.normal) < 0)
				continue;*/

			//color += get_material_phong_color(hit, t);// * (hit.material->emission + get_material_brdf_mirror_color(hit, t));
			auto ray = cast_ray(hit.hit, dir, t);
			Vector3 result{ 0,0,0 };
			if (!get_ray_color(ray, t, result, hit.n1, bump, &Raytracer::get_material_emission_color))
			//if (!get_ray_color(ray, t, result, hit.n1, -1, &Raytracer::get_material_brdf_phong_color))
				result = cubeMap_->get_texel(Vector3(ray.ray.dir_x, ray.ray.dir_y, ray.ray.dir_z));
				//result = { 1,1,1 };
				//result = { 0,0,0 };
			//get_ray_color(ray, t, result, hit.n1, bump, &Raytracer::get_material_brdf_phong_color);
			color += result * u / pdf;
		}
	return color / pow(ax_splits, 2);

	/*auto ray = cast_ray(hit.hit, hit.hit + hit.normal, t);
	return get_material_brdf_ray_color(ray, t, hit.material->ior);*/
}

RTCRay Raytracer::generate_ray(const Vector3& hit, const Vector3& direction)
{
	RTCRay ray;

	ray.mask = 0; // can be used to mask out some geometries for some rays
	ray.id = 0; // identify a ray inside a callback function
	ray.flags = 0; // reserved

	ray.org_x = hit.x;
	ray.org_y = hit.y;
	ray.org_z = hit.z;

	ray.dir_x = direction.x;
	ray.dir_y = direction.y;
	ray.dir_z = direction.z;

	return ray;
}

RTCRayHit Raytracer::cast_ray(const Vector3& position, const Vector3& direction, const float& t, const float& tnear)
{
	RTCRayHit ray_hit = prepare_ray_hit(t, generate_ray(position, direction), tnear);
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);
	rtcIntersect1(scene_, &context, &ray_hit);
	return ray_hit;
}

RTCRayHit Raytracer::cast_ray(const RTCRay& ray, const float& t)
{
	RTCRayHit ray_hit = prepare_ray_hit(t, ray);
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);
	rtcIntersect1(scene_, &context, &ray_hit);
	return ray_hit;
}

RTCRayHitModel Raytracer::build_ray_model(const RTCRayHit& hit, const float& ior)
{
	return RTCRayHitModel(hit, &scene_, ior);
}

bool Raytracer::has_colision(const RTCRayHit& hit)
{
	return hit.hit.geomID != RTC_INVALID_GEOMETRY_ID;
}

bool Raytracer::has_colision(const RTCRayHitModel& hit)
{
	return hit.material->shader != 4;
}

RayCollision Raytracer::get_collision_type(RTCRayHitModel& hit, const int bump)
{
	RayCollision collision = Diffuse;
	if (bump <= RAY_MAX_BUMPS)
	{
		if (refl_ 
			&& refr_
			&& hit.material->isReflective()
			&& hit.material->isTransparent())
		{
			hit.calc_fresnel();
			if (hit.R == 0)
				collision = Refraction;
			else if (hit.R == 1)
				collision = Reflection;
			else
				collision = All;
		}
		else if (refl_ && hit.material->isReflective())
		{
			hit.calc_fresnel();
			collision = Reflection;
		}
		else if (refr_ && hit.material->isTransparent())
		{
			hit.calc_fresnel();
			collision = Refraction;
		}
		/*else if (brdf_)
			hit.calc_fresnel();*/
	}
	/*else if(brdf_)
		hit.calc_fresnel();*/

	if (ray_map_ && bump == RAY_MAP_BUMP)
		collision = RayMap;

	return collision;
}

int Raytracer::get_ray_count(RTCRayHit ray_hit, const float& t, float& n1, int bump)
{
	int count = 0;
	if (has_colision(ray_hit))
	{
		auto data = build_ray_model(ray_hit, n1);
		bump++;
		switch (get_collision_type(data, bump))
		{
		case Diffuse:
			count++;
			break;

		case RayMap:
		case All:
			// Refraction
			count += get_ray_count(cast_ray(data.hit, data.refracted, t), t, data.n2, bump);

			// Reflection
			count += get_ray_count(cast_ray(data.hit, data.reflected, t), t, data.n1, bump) + 1;
			break;

		case Refraction:
			count = get_ray_count(cast_ray(data.hit, data.refracted, t), t, data.n2, bump);
			break;

		case Reflection:
			count = get_ray_count(cast_ray(data.hit, data.reflected, t), t, data.n1, bump) + 1;
			break;
		}
	}
	return count;
}

bool Raytracer::get_ray_color(RTCRayHit ray_hit, const float& t, Vector3& color, float& n1, int bump, Vector3 (Raytracer::*sample_func)(RTCRayHitModel&, const float&))
{
	// intersected ray with the scene

	if (has_colision(ray_hit))
	{
		auto data = build_ray_model(ray_hit, n1);
		bump++;
		float distance = data.core.ray.tfar;
		switch (get_collision_type(data, bump))
		{
			case Diffuse:
				color = (*this.*sample_func)(data, t);
				break;

			case All:
				// Refraction
				if (!get_ray_color(cast_ray(data.hit, data.refracted, t), t, data.colorRefracted, data.n2, bump, sample_func))
					data.colorRefracted = cubeMap_->get_texel(data.refracted);
				else distance = 0;

				// Reflection
				if (!get_ray_color(cast_ray(data.hit, data.reflected, t), t, data.colorReflected, data.n1, bump, sample_func))
					data.colorReflected = cubeMap_->get_texel(data.reflected);

				// Result
				color = data.calc_result_color(distance);
				break;

			case Refraction:
				if (!get_ray_color(cast_ray(data.hit, data.refracted, t), t, data.colorRefracted, data.n2, bump, sample_func))
					data.colorRefracted = cubeMap_->get_texel(data.refracted);
				else distance = 0;
				data.colorReflected = { 0, 0, 0 };
				color = data.calc_result_color(distance);
				break;

			case Reflection:
				if(!get_ray_color(cast_ray(data.hit, data.reflected, t), t, data.colorReflected, data.n1, bump, sample_func))
					data.colorReflected = cubeMap_->get_texel(data.reflected);
				if(data.R != 0)
					data.colorRefracted = (*this.*sample_func)(data, t);
				else
					data.colorRefracted = { 0, 0, 0 };
				color = data.calc_result_color(-1);
				break;

			case RayMap:
				float count = (float)get_ray_count(ray_hit, t, n1, bump - 1) + 1;
				count = count / (float)pow(RAY_MAX_BUMPS, 1 + refl_ + refr_);
				// cout, refracted, reflected
				color = Vector3(count, data.R, 1.f-data.R);
				break;
		}
		return true;
	}

	return false;
}

Vector3 Raytracer::get_pixel_internal(const int x, const int y, const int t)
{
	Vector3 color(0, 0, 0);
	auto ray = cast_ray(camera_.GenerateRay(x, y), t);
	auto ior = IOR_AIR;
	if (!get_ray_color(ray, t, color, ior, 0, &Raytracer::get_material_color))
		// Background
		color = cubeMap_->get_texel(Vector3(ray.ray.dir_x, ray.ray.dir_y, ray.ray.dir_z));
	return color;
}

Color4f Raytracer::get_pixel( const int x, const int y, const float t )
{
	auto start = begin();

	Vector3 color(0,0,0);
	if (ss_ == 0)
		color = get_pixel_internal(x, y, t);
	else
	{
		int count = 0;
		for (int i = -ss_; i <= ss_; i++)
			for (int j = -ss_; j <= ss_; j++)
			{
				const float nx = get_random_ss_float(), ny = get_random_ss_float();
				const float dx = i * (SS_MD/ss_) + nx/ss_, dy = j * (SS_MD/ss_) + (ny/ss_);
				//const float dx = i * 0.25f, dy = j * 0.25f;
				color += get_pixel_internal(x + dx, y + dy, t);
				count++;
			}
		color /= (float)count;
	}
	color = SrgbTransform::linearToSrgb(SrgbTransform::tonemap(color));

	if (debug_)
	{
		log(start, "get_pixel");

		if (x == 1)
		{
			#pragma omp critical
			{
				auto max = 0.0f;
				for (auto& x : times)
					if (x.second > max)
						max += x.second;
				times_text.clear();
				for (auto& x : times)
					times_text.append(std::to_string((int)(x.second / max * 100.f)) + " %\t\t" + x.first + "\t\t\t" + std::to_string(x.second / 1000.0f) + " s\n");
			}
		}
	}

	return Color4f{ color.x, color.y, color.z, 1 };
}

float Raytracer::get_random_float()
{
	return dist_(e2_);
}

float Raytracer::get_random_ss_float()
{
	return ss_dist_(ss_e2_);
}

int Raytracer::Ui()
{
	//static float f = 0.0f;
	static int counter = 0;

	// Use a Begin/End pair to created a named window
	ImGui::Begin("Ray Tracer Params");


	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::ProgressBar(progress());
	ImGui::Text("Progress = %d / %d\t[%d x %d]", current(), width(), width(), height());
	ImGui::Text("Time = Done: %.2f s \t Left: %.2f s", lastFrame_.count(), (lastFrame_.count() / current()) * (width() - current()));
	//ImGui::Text("Time = %.2f", lastFrame_.count());
	ImGui::Text("Surfaces = %d", surfaces_.size());
	ImGui::Text("Materials = %d", materials_.size());
	ImGui::Separator();
	ImGui::Checkbox("Vsync", &vsync_);
	ImGui::Checkbox("Save", &save_);
	ImGui::Separator();
	//ImGui::Checkbox("Debug", &debug_);
	ImGui::Checkbox("Cubemap texture", &cubeMap_->returnTexture);
	ImGui::Checkbox("Light map", &light_map_);
	ImGui::Checkbox("Ray map", &ray_map_);
	ImGui::Checkbox("Shadows", &shadows_);
	ImGui::Checkbox("Reflection", &refl_);
	ImGui::Checkbox("Refraction", &refr_);
	ImGui::Separator();
	ImGui::Checkbox("BRDF", &brdf_);
	ImGui::Separator();
	
	//ImGui::Checkbox( "Demo Window", &show_demo_window ); // Edit bools storing our window open/close state
	//ImGui::Checkbox( "Another Window", &show_another_window );
	if(ray_map_)
		ImGui::SliderInt("Ray map depth", &RAY_MAP_BUMP, 0, RAY_MAX_BUMPS);
	if (light_map_)
		ImGui::SliderInt("Light map depth", &LIGHT_MAP_BUMP, 0, RAY_MAX_BUMPS);
	ImGui::SliderInt("Ray depth", &RAY_MAX_BUMPS, 0, 20);
	ImGui::SliderInt("BRDF Samples", &BRDF_SAMPLES_EXP, 0, 10);
	ImGui::SliderInt("Super Sampling", &ss_, 0, 9); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::Separator();
	if (!cubeMap_->returnTexture)
	{
		ImGui::SliderFloat("Cubemap R", &cubeMap_->color.x, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
		ImGui::SliderFloat("Cubemap G", &cubeMap_->color.y, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
		ImGui::SliderFloat("Cubemap B", &cubeMap_->color.z, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
		ImGui::Separator();
	}
	ImGui::SliderFloat("Light X", &light_.x, -1000.0f, 1000.0f ); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Light Y", &light_.y, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Light Z", &light_.z, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Light Power R", &lightPower_.x, 0.f, 1.f); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Light Power G", &lightPower_.y, 0.f, 1.f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Light Power B", &lightPower_.z, 0.f, 1.f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::Separator();
	ImGui::SliderFloat("Camera X", &camera_.view_from_.x, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Camera Y", &camera_.view_from_.y, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Camera Z", &camera_.view_from_.z, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Target X", &camera_.view_at_.x, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Target Y", &camera_.view_at_.y, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Target Z", &camera_.view_at_.z, -1000.0f, 1000.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	//ImGui::ColorEdit3( "clear color", ( float* )&clear_color ); // Edit 3 floats representing a color

	// Buttons return true when clicked (most widgets return true when edited/activated)
	if (ImGui::Button("Update Target"))
		camera_.Update();
		//counter++;
	/*ImGui::SameLine();
	ImGui::Text( "counter = %d", counter );*/
	//ImGui::Text("Functions:\n%s", times_text.c_str());

	ImGui::End();

	// 3. Show another simple window.
	/*if ( show_another_window )
	{
	ImGui::Begin( "Another Window", &show_another_window ); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
	ImGui::Text( "Hello from another window!" );
	if ( ImGui::Button( "Close Me" ) )
	show_another_window = false;
	ImGui::End();
	}*/

	return 0;
}