#pragma once
#include "simpleguidx11.h"
#include "surface.h"
#include "camera.h"
#include "cubemap.h"
#include "RTCRayHitModel.h"
#include "RayCollision.h"

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
		Vector3* light, Vector3* lightPower,
		const Vector3* background = nullptr,
		const char * config = "threads=0,verbose=3");
	~Raytracer();

	int InitDeviceAndScene( const char * config );

	int ReleaseDeviceAndScene();

	void LoadScene( const std::string file_name );
	Vector3 get_material_color(RTCRayHitModel& hit, const float& t);
	Vector3 get_material_emission_color(RTCRayHitModel& hit, const float& t);
	Vector3 get_material_phong_color(RTCRayHitModel& hit, const float& t);
	Vector3 get_material_brdf_mirror_color(RTCRayHitModel& hit, const float& t);
	Vector3 get_material_brdf_phong_color(RTCRayHitModel& hit, const float& t);
	Vector3 get_material_brdf_ray_color(RTCRayHit& ray, const float& t, const float& ior, int bump = 0);
	Vector3 get_material_brdf_color(RTCRayHitModel& hit, const float& t, int bump = 0);
	//void get_geometry_data(RTCRayHit& ray_hit, Vector3& normalVec, Coord2f& tex_coord, Material*& material);

	bool get_ray_color(RTCRayHit ray_hit, const float& t, Vector3& color, float& n1, int bump, Vector3(Raytracer::*sample_func)(RTCRayHitModel&, const float&));
	Vector3 get_pixel_internal(int x, int y, int t);
	Color4f get_pixel( const int x, const int y, const float t = 0.0f ) override;
	float get_random_float();
	float get_random_ss_float();
	RTCRayHit prepare_ray_hit(float t, RTCRay ray, const float& tnear = 0.1f);
	RTCRay generate_ray(const Vector3& hit, const Vector3& direction);
	RTCRayHit cast_ray(const Vector3& position, const Vector3& direction, const float& t, const float& tnear = 0.1f);
	RTCRayHit cast_ray(const RTCRay& ray, const float& t);
	RTCRayHitModel build_ray_model(const RTCRayHit& hit, const float& ior);
	static bool has_colision(const RTCRayHit& hit);
	static bool has_colision(const RTCRayHitModel& hit);
	RayCollision get_collision_type(RTCRayHitModel& hit, const int bump);
	int get_ray_count(RTCRayHit ray_hit, const float& t, float& n1, int bump);

	int Ui();

private:
	std::random_device rd_;
	std::mt19937 e2_;
	uniform_real_distribution<> dist_;

	std::mt19937 ss_e2_;
	uniform_real_distribution<> ss_dist_;

	std::vector<Surface *> surfaces_;
	std::vector<Material *> materials_;

	RTCDevice device_;
	RTCScene scene_;
	Camera camera_;
	Vector3 light_;
	Vector3 lightPower_;
	CubeMap* cubeMap_;

	bool shadows_{ true };

	float MAX_RAYS = 3628800.f;
	int RAY_MAX_BUMPS = 0;
	bool refr_{ true };
	bool refl_{ true };

	int RAY_MAP_BUMP = 0;
	bool ray_map_{ false };

	int LIGHT_MAP_BUMP = 0;
	bool light_map_{ false };

	float SS_D = 0.1f, SS_MD = 0.25f;
	int ss_ = 0;

	int BRDF_SAMPLES_EXP = 5;
	bool brdf_{ false };

	int done_ = 0;
	float f_, rendered_ = 0;

	string times_text = "";
	map<string, float> times;
	chrono::time_point<chrono::steady_clock> begin();
	void log(chrono::time_point<chrono::steady_clock>& begin, string prefix);
	void log(chrono::time_point<chrono::steady_clock>& begin, string prefix, int bump);
};