#pragma once
#include <string>
#include "texture.h"
#include <list>
#include "vector3.h"


using namespace std;

class CubeMap
{
	public:

		vector<Texture*> textures;

		CubeMap(const char* posx, const char* negx, const char* posy,
			const char* negy, const char* posz, const char* negz);
	
		Vector3 get_texel(Vector3 direction);
};

