#include "stdafx.h"
#include "raytracer.h"
#include "objloader.h"
#include "tutorials.h"
#include <math.h>
#include "SrgbTransform.h"
#include <chrono>
#include <iostream>

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


Raytracer::Raytracer( const int width, const int height,
	const float fov_y, const Vector3 view_from, const Vector3 view_at,
	const char * config ) : SimpleGuiDX11( width, height )
{
	InitDeviceAndScene( config );

	camera_ = Camera( width, height, fov_y, view_from, view_at );
	
	cubeMap_ = new CubeMap("../../../data/sky185/sky185rt.png",
		"../../../data/sky185/sky185lf.png", "../../../data/sky185/sky185bk.png",
		"../../../data/sky185/sky185ft.png", "../../../data/sky185/sky185up.png",
		"../../../data/sky185/sky185dn.png");

	e2_ = std::mt19937(rd_());
	dist_ = std::uniform_real_distribution<>(-0.5f, 0.5f);

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

	light_ = Vector3{ 200,300,400 };
	light_.Normalize();
	lightPower_ = Vector3{ 1.0f, 1.0f, 1.0f };

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


RTCRayHit Raytracer::prepare_ray_hit(const float t, RTCRay ray)
{
	ray.tnear = 0.1f;// FLT_MIN; // start of ray segment
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

Vector3 Raytracer::get_material_color(Vector3 &normalVec, Coord2f &tex_coord, Material* material, Vector3 &hit, Vector3 &origin)
{
	Vector3 color = Vector3{ 0.5,0.2,0.55 };
	if(material != nullptr)
	{
		// Check Shadow
		auto ray_hit = prepare_ray_hit(0, generate_ray(hit, light_));
		RTCIntersectContext context;
		rtcInitIntersectContext(&context);
		rtcIntersect1(scene_, &context, &ray_hit);

		bool shadow = false;

		if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
		{
			Vector3 normal;
			Coord2f texture;
			Material* mat;
			get_geometry_data(ray_hit, normal, texture, mat);
			if (mat->alpha == 1.f)
				shadow = true;
		}

		if(shadow)
			color = lightPower_.x * material->ambient;
		else
		{
			// Get Difuse
			color = material->diffuse;
			Texture* difuse = material->get_texture(material->kDiffuseMapSlot);
			if (difuse != nullptr)
			{
				Color3f texlet = difuse->get_texel(tex_coord.u, 1.0f - tex_coord.v);
				color.x = texlet.r;
				color.y = texlet.g;
				color.z = texlet.b;
				//color.Normalize();
			}

			//return color;

			// Compute lighting
			Vector3 reflectedVec = light_.Reflect(normalVec);//2 * (light_.CrossProduct(normalVec))*normalVec - light_;
			reflectedVec.Normalize();
			Vector3 cam = origin;
			cam.Normalize();

			Vector3 specular = material->specular;
			/*Texture* specularmap = material->get_texture(material->kSpecularMapSlot);
			if (specularmap != nullptr)
			{
				Color3f texlet = specularmap->get_texel(tex_coord.u, 1.0f - tex_coord.v);
				specular.x *= texlet.r;
				specular.y *= texlet.g;
				specular.z *= texlet.b;
				specular.Normalize();
			}*/

			// Rotate normal of black transparent sides
			color = lightPower_.x * material->ambient +
				lightPower_.y * color * max(normalVec.DotProduct(light_), .0f) +
				lightPower_.z * specular * powf(max(reflectedVec.DotProduct(cam), .0f), material->shininess);
		}
	}

	return SrgbTransform::srgbToLinear(color);
}

void Raytracer::get_geometry_data(RTCRayHit& ray_hit, Vector3& normalVec, Coord2f& tex_coord, Material*& material)
{
	// we hit something
	RTCGeometry geometry = rtcGetGeometry(scene_, ray_hit.hit.geomID);
	Normal3f normal;
		
	// get interpolated normal
	rtcInterpolate0(geometry, ray_hit.hit.primID, ray_hit.hit.u, ray_hit.hit.v,	RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, &normal.x, 3);
	normalVec = { normal.x, normal.y, normal.z };
	if (normalVec.DotProduct(Vector3(ray_hit.ray.org_x, ray_hit.ray.org_y, ray_hit.ray.org_z)) < 0)
		normalVec = { -normal.x, -normal.y, -normal.z };

	rtcInterpolate0(geometry, ray_hit.hit.primID, ray_hit.hit.u, ray_hit.hit.v,	RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 1, &tex_coord.u, 2);

	material = static_cast<Material*>(rtcGetGeometryUserData(geometry));
}

RTCRay Raytracer::generate_ray(Vector3& hit, Vector3& direction)
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

bool Raytracer::get_ray_color(RTCRayHit& ray_hit, const float& t, Vector3& color, float& n1, int bump)
{

	// intersect ray with the scene
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);
	rtcIntersect1(scene_, &context, &ray_hit);

	if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
	{
		Vector3 from(ray_hit.ray.org_x, ray_hit.ray.org_y, ray_hit.ray.org_z);
		Vector3 hit = from + ray_hit.ray.tfar * Vector3(ray_hit.ray.dir_x, ray_hit.ray.dir_y, ray_hit.ray.dir_z);
		Vector3 normalVec;
		Coord2f tex_coord;
		Material* material;
		get_geometry_data(ray_hit, normalVec, tex_coord, material);
		
		if (bump < RAY_MAX_BUMPS)// && material->reflectivity > 0.f)
		{
			bump++;

			//Prepare next ray data
			Vector3 dir(ray_hit.ray.dir_x, ray_hit.ray.dir_y, ray_hit.ray.dir_z);
			Vector3 hitNormal(ray_hit.hit.Ng_x, ray_hit.hit.Ng_y, ray_hit.hit.Ng_z);
			hitNormal.Normalize();

			Vector3 resultReflected, resultRefracted;

			Vector3 reflectedVec = (-dir).Reflect(normalVec);
			reflectedVec.Normalize();

			bool reflBaseColor = false;

			auto start = begin();
			if (refl_)
			{
				//Try reflected ray
				if (!get_ray_color(*&prepare_ray_hit(t, generate_ray(hit, reflectedVec)), t, resultReflected, material->ior, bump))
					resultReflected = cubeMap_->get_texel(reflectedVec);
			}
			else
			{
				reflBaseColor = true;
				resultReflected = get_material_color(normalVec, tex_coord, material, hit, from);
			}
			log(start, "reflection", bump-1);

			if (n1 <= 0.f)
				n1 = IOR_AIR;
			float R = 0.f, len = ray_hit.ray.tfar, n2 = n1 > IOR_AIR ? IOR_AIR : material->ior, n12 = n1 / n2;

			start = begin();
			if (refr_ && material->alpha < 1.f)
			{
				//Try refracted ray
				float dirNormal = dir.DotProduct(normalVec);
				auto a = powf(dirNormal, 2.0f),
					b = sqrt(1.f - powf(n12, 2.f) * (1.f - a));
				auto c = (n12 * dirNormal + b) * (normalVec);
				Vector3 refractedVec = n12 * dir - c;

				// Refraction
				float
					R0 = powf((n1 - n2) / (n1 + n2), 2.f),
					o = n1 <= n2 ? dir.DotProduct(normalVec) : refractedVec.DotProduct(normalVec);
				R = max(R0 + (1.f - R0) * powf(1.f - cosf(o), 5.f), 0.01f);
				/*float o1 = dir.DotProduct(normalVec), o2 = refractedVec.DotProduct(-normalVec),
					n2o2 = n2 * cos(o2), n1o1 = n1 * cos(o1),
					n2o1 = n2 * cos(o1), n1o2 = n1 * cos(o2),
					Rs = powf((n2o2 - n1o1) / (n2o2 + n1o1), 2.f),
					Rp = powf((n2o1 - n1o2) / (n2o1 + n1o2), 2.f);
				R = (Rs + Rp) / 2.f;*/

				if (!get_ray_color(*&prepare_ray_hit(t, generate_ray(hit, refractedVec)), t, resultRefracted, n2, bump))
					resultRefracted = cubeMap_->get_texel(refractedVec);
				else len = 0; 

				// Refraction + Reflection + Attenuation
				color = (resultRefracted * (1.f - R) + resultReflected * R) * material->attenuation.Exp(-len);
			}
			else
			{
				// Reflection only
				float
					R0 = powf((n1 - n2) / (n1 + n2), 2.f),
					o = dir.DotProduct(normalVec);
				R = R0 + (1.f - R0) * powf(1.f - cosf(o), 5.f);
				if (reflBaseColor)
					color = resultReflected;
				else
					color = get_material_color(normalVec, tex_coord, material, hit, from);
				color += resultReflected * R;
			}
			log(start, "refraction", bump-1);
		}
		else
		{
			color = get_material_color(normalVec, tex_coord, material, hit, from);
		}

		return true;
	}

	return false;
}

Vector3 Raytracer::get_pixel_internal(const int x, const int y, const int t)
{
	Vector3 color(0, 0, 0);
	auto ray = camera_.GenerateRay(x, y);
	auto ior = IOR_AIR;
	if (!get_ray_color(*&prepare_ray_hit(t, ray), t, color, ior, 0))
		// Background
		color = cubeMap_->get_texel(Vector3(ray.dir_x, ray.dir_y, ray.dir_z));
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
				//const float dx = get_random_float(), dy = get_random_float();
				const float dx = i * 0.25f, dy = j * 0.25f;
				color += get_pixel_internal(x + dx, y + dy, t);
				count++;
			}
		color /= (float)count;
	}
	color = SrgbTransform::linearToSrgb(color);

	if (debug_)
	{
		log(start, "get_pixel");

		if (x == 1)
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

	return Color4f{ color.x, color.y, color.z, 1 };
}

float Raytracer::get_random_float()
{
	return dist_(e2_);
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
	ImGui::Checkbox("Debug", &debug_);
	ImGui::Checkbox("Save", &save_);
	ImGui::Checkbox("Vsync", &vsync_ );
	ImGui::Checkbox("Reflection", &refl_);
	ImGui::Checkbox("Refraction", &refr_);
	
	//ImGui::Checkbox( "Demo Window", &show_demo_window ); // Edit bools storing our window open/close state
	//ImGui::Checkbox( "Another Window", &show_another_window );
	
	ImGui::SliderInt("Super Sampling", &ss_, 0, 9); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Lx", &light_.x, -1.0f, 1.0f ); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Ly", &light_.y, -1.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Lz", &light_.z, -1.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Cx", &camera_.view_from_.x, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Cy", &camera_.view_from_.y, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Cz", &camera_.view_from_.z, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Tx", &camera_.view_at_.x, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Ty", &camera_.view_at_.y, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Tz", &camera_.view_at_.z, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderInt("Bumps", &RAY_MAX_BUMPS, 0, 10);
	//ImGui::ColorEdit3( "clear color", ( float* )&clear_color ); // Edit 3 floats representing a color

	// Buttons return true when clicked (most widgets return true when edited/activated)
	if (ImGui::Button("Update Target"))
		camera_.Update();
		//counter++;
	/*ImGui::SameLine();
	ImGui::Text( "counter = %d", counter );*/
	ImGui::Text("Functions:\n%s", times_text.c_str());

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