#pragma once
#include "simpleguidx11.h"
#include "surface.h"
#include "camera.h"

/*! \class Raytracer
\brief General ray tracer class.

\author Tomáš Fabián
\version 0.1
\date 2018
*/
class Raytracer : public SimpleGuiDX11
{
public:
	Raytracer( const int width, const int height, 
		const float fov_y, const Vector3 view_from, const Vector3 view_at,
		const char * config = "threads=0,verbose=3" );
	~Raytracer();

	int InitDeviceAndScene( const char * config );

	int ReleaseDeviceAndScene();

	void LoadScene( const std::string file_name );
	RTCRayHit prepare_ray_hit(float t, RTCRay ray);
	Vector3 get_material_color(Vector3 normalVec, Coord2f tex_coord, Material* material, Vector3 origin);
	void get_geometry_data(RTCRayHit ray_hit, Vector3& normalVec, Coord2f& tex_coord, Material*& material);

	bool get_ray_color(RTCRayHit ray_hit, const float t, Vector3& color, int bump);
	Color4f get_pixel( const int x, const int y, const float t = 0.0f ) override;

	int Ui();

private:
	std::vector<Surface *> surfaces_;
	std::vector<Material *> materials_;

	RTCDevice device_;
	RTCScene scene_;
	Camera camera_;
	Vector3 light_;
	Vector3 lightPower_;
	int RAY_MAX_BUMPS = 5;

	float f_;
};
