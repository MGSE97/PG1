#include "stdafx.h"
#include "raytracer.h"
#include "objloader.h"
#include "tutorials.h"

Raytracer::Raytracer( const int width, const int height,
	const float fov_y, const Vector3 view_from, const Vector3 view_at,
	const char * config ) : SimpleGuiDX11( width, height )
{
	InitDeviceAndScene( config );

	camera_ = Camera( width, height, fov_y, view_from, view_at );
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
	ray.tnear = FLT_MIN; // start of ray segment
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

Vector3 Raytracer::get_material_color(Vector3 normalVec, Coord2f tex_coord, Material* material)
{
	Vector3 color = Vector3{ 0.5,0.2,0.55 };
	if(material != nullptr)
	{
		// Get Difuse
		color = material->diffuse;
		Texture* difuse = material->get_texture(material->kDiffuseMapSlot);
		if (difuse != nullptr)
		{
			Color3f texlet = difuse->get_texel(tex_coord.u, 1.0f - tex_coord.v);
			color.x *= texlet.r;
			color.y *= texlet.g;
			color.z *= texlet.b;
			color.Normalize();
		}

		// Compute lighting
		Vector3 reflectedVec = light_.Reflect(normalVec);//2 * (light_.CrossProduct(normalVec))*normalVec - light_;
		reflectedVec.Normalize();
		Vector3 cam = camera_.view_from_;
		cam.Normalize();

		Vector3 specular = material->specular;
		Texture* specularmap = material->get_texture(material->kSpecularMapSlot);
		if (specularmap != nullptr)
		{
			Color3f texlet = specularmap->get_texel(tex_coord.u, 1.0f - tex_coord.v);
			specular.x *= texlet.r;
			specular.y *= texlet.g;
			specular.z *= texlet.b;
			specular.Normalize();
		}
			
		color = lightPower_.x * material->ambient +
			lightPower_.y * color * max(normalVec.DotProduct(light_), .0f) +
			lightPower_.z * specular * powf(max(reflectedVec.DotProduct(cam), .0f), material->shininess);
	}

	return color;
}

void Raytracer::get_geometry_data(RTCRayHit ray_hit, Vector3& normalVec, Coord2f& tex_coord, Material*& material)
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

RTCRay generate_ray(Vector3& hit, Vector3& direction)
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

bool Raytracer::get_ray_color(RTCRayHit ray_hit, const float t, Vector3& color, int bump)
{
	// intersect ray with the scene
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);
	rtcIntersect1(scene_, &context, &ray_hit);

	if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
	{
		Vector3 normalVec;
		Coord2f tex_coord;
		Material* material;
		get_geometry_data(ray_hit, normalVec, tex_coord, material);

		color = get_material_color(normalVec, tex_coord, material);
		//color = Vector3(ray_hit.hit.geomID/100.f, bump*25.f, 0);
		//color = Vector3(0, bump*25.f, 0);
		
		if (bump < RAY_MAX_BUMPS && material->reflectivity > 0.f)
		{
			//Try reflected ray
			//Vector3 normal(ray_hit.hit.Ng_x, ray_hit.hit.Ng_y, ray_hit.hit.Ng_z);
			Vector3 normal(ray_hit.hit.Ng_x, ray_hit.hit.Ng_y, ray_hit.hit.Ng_z);
			normal.Normalize();
			if (normal.DotProduct(Vector3(ray_hit.ray.org_x, ray_hit.ray.org_y, ray_hit.ray.org_z)) < 0)
				normal = { -normal.x, -normal.y, -normal.z };
			Vector3 hit = Vector3(ray_hit.ray.org_x, ray_hit.ray.org_y, ray_hit.ray.org_z) + ray_hit.ray.tfar * Vector3(ray_hit.ray.dir_x, ray_hit.ray.dir_y, ray_hit.ray.dir_z);
			//Vector3 dir(-ray_hit.ray.dir_x, -ray_hit.ray.dir_y, -ray_hit.ray.dir_z);
			//Vector3 dir(ray_hit.ray.dir_x, ray_hit.ray.dir_y, ray_hit.ray.dir_z);
			Vector3 dir(ray_hit.ray.org_x, ray_hit.ray.org_y, ray_hit.ray.org_z);
			Vector3 reflectedVec = dir.Reflect(normal);
			reflectedVec.Normalize();
			Vector3 reflected;
			if (get_ray_color(prepare_ray_hit(t, generate_ray(hit, reflectedVec)), t, reflected, ++bump) && reflected.x > 0 && reflected.y > 0 && reflected.z > 0)
			{
				float p = powf(material->reflectivity, bump);
				color = color * (1.f-p) + reflected * p;
			}
		}

		return true;
	}

	return false;
}

Color4f Raytracer::get_pixel( const int x, const int y, const float t )
{
	// TODO generate primary ray and perform ray cast on the scene
	Vector3 color;
	if (get_ray_color(prepare_ray_hit(t, camera_.GenerateRay(x, y)), t, color, 0))
		return Color4f{ color.x, color.y, color.z, 1 };
	// Background
	return Color4f{ 0.2,0.2,0.2,1 };
	//// intersect ray with the scene
	//RTCIntersectContext context;
	//rtcInitIntersectContext(&context);
	//rtcIntersect1(scene_, &context, &ray_hit);

	//if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
	//{
	//	Vector3 normalVec;
	//	Coord2f tex_coord;
	//	Material* material;
	//	get_geometry_data(ray_hit, normalVec, tex_coord, material);


	//	Color4f color = get_material_color(normalVec, tex_coord, material);
	//	do
	//	{
	//		Vector3 hit(ray_hit.hit.Ng_x, ray_hit.hit.Ng_y, ray_hit.hit.Ng_z);
	//		Vector3 dir(ray_hit.ray.dir_x, ray_hit.ray.dir_y, ray_hit.ray.dir_z);
	//		Vector3 reflectedVec = dir.Reflect(normalVec);
	//		ray_hit = prepare_ray_hit(t, generate_ray(hit, reflectedVec));

	//		// intersect ray with the scene
	//		rtcInitIntersectContext(&context);
	//		rtcIntersect1(scene_, &context, &ray_hit);

	//		if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
	//		{
	//			get_geometry_data(ray_hit, normalVec, tex_coord, material);
	//			color = get_material_color(normalVec, tex_coord, material);
	//		}
	//	} while (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID);

		//return color;
	//}

	//return Color4f{ 0.2,0.2,0.2,1 };
}

int Raytracer::Ui()
{
	//static float f = 0.0f;
	static int counter = 0;

	// Use a Begin/End pair to created a named window
	ImGui::Begin( "Ray Tracer Params" );
	
	ImGui::Text( "Surfaces = %d", surfaces_.size() );
	ImGui::Text( "Materials = %d", materials_.size() );
	ImGui::Separator();
	ImGui::Checkbox( "Vsync", &vsync_ );
	
	//ImGui::Checkbox( "Demo Window", &show_demo_window ); // Edit bools storing our window open/close state
	//ImGui::Checkbox( "Another Window", &show_another_window );

	ImGui::SliderFloat( "Lx", &light_.x, -1.0f, 1.0f ); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Ly", &light_.y, -1.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Lz", &light_.z, -1.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Cx", &camera_.view_from_.x, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f    
	ImGui::SliderFloat("Cy", &camera_.view_from_.y, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderFloat("Cz", &camera_.view_from_.z, -400.0f, 400.0f); // Edit 1 float using a slider from 0.0f to 1.0f   
	ImGui::SliderInt("Bumps", &RAY_MAX_BUMPS, 0, 10);
	//ImGui::ColorEdit3( "clear color", ( float* )&clear_color ); // Edit 3 floats representing a color

	// Buttons return true when clicked (most widgets return true when edited/activated)
	if ( ImGui::Button( "Button" ) )
		counter++;
	ImGui::SameLine();
	ImGui::Text( "counter = %d", counter );

	ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate );
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
