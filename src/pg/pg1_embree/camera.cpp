#include "stdafx.h"
#include "camera.h"

Camera::Camera( const int width, const int height, const float fov_y,
	const Vector3 view_from, const Vector3 view_at )
{
	width_ = width;
	height_ = height;
	fov_y_ = fov_y;
	area_ = width * height;

	view_from_ = view_from;
	view_at_ = view_at;
	Update();
}

Camera* Camera::Update()
{
	// TODO compute focal lenght based on the vertical field of view and the camera resolution
	f_y_ = height_ / (2 * tanf(fov_y_) * 0.5f);

	// TODO build M_c_w_ matrix	
	Vector3 up = Vector3(0, 0, 1);
	Vector3 zc = view_from_ - view_at_;//.CrossProduct(up);
	Vector3 xc = up.CrossProduct(zc);
	Vector3 yc = zc.CrossProduct(xc);
	xc.Normalize();
	yc.Normalize();
	zc.Normalize();
	M_c_w_ = Matrix3x3(xc, yc, zc);

	return this;
}

RTCRay Camera::GenerateRay( const float x_i, const float y_i ) const
{
	RTCRay ray = RTCRay();

	ray.mask = 0; // can be used to mask out some geometries for some rays
	ray.id = 0; // identify a ray inside a callback function
	ray.flags = 0; // reserved

	// TODO fill in ray structure and compute ray direction
	// ray.org_x = ...	
	ray.org_x = view_from_.x;
	ray.org_y = view_from_.y;
	ray.org_z = view_from_.z;

	Vector3 d = Vector3(x_i - width_ * 0.5f, height_ * 0.5f - y_i, -f_y_);
	Vector3 dw = M_c_w_ * d;
	dw.Normalize();
	ray.dir_x = dw.x;
	ray.dir_y = dw.y;
	ray.dir_z = dw.z;

	return ray;
}
