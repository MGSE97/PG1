#pragma once
#include <string>
#include "texture.h"
#include <list>
#include "vector3.h"
#include "Color.h"

using namespace std;

class CubeMap
{
	public:
		bool returnTexture{ true };
		Vector3 color = { 1,1,1 };
		Vector3 emission = { 0,0,0 };
		vector<Texture*> textures;

		CubeMap(const char* posx, const char* negx, const char* posy,
			const char* negy, const char* posz, const char* negz);


		CubeMap(const char* posx, const char* negx, const char* posy,
			const char* negy, const char* posz, const char* negz, const Vector3 color);

		CubeMap(const Vector3 color = { 1,1,1 });
	
		Vector3 get_texel(Vector3 direction);
		Color get_texel_color(Vector3 direction);
};

