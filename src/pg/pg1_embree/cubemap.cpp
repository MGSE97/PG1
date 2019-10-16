#include "stdafx.h"
#include "CubeMap.h"

CubeMap::CubeMap(const char* posx, const char* negx, const char* posy,
				const char* negy, const char* posz, const char* negz)
{
	textures.push_back(new Texture(posx));
	textures.push_back(new Texture(negx));
	textures.push_back(new Texture(posy));
	textures.push_back(new Texture(negy));
	textures.push_back(new Texture(posz));
	textures.push_back(new Texture(negz));
}

Vector3 CubeMap::get_texel(Vector3 direction)
{
	auto ret = direction.LargestComponent(true);
	
	int position;
	float u, v;
	float tmp;

	switch (ret)
	{
	case 0:
		tmp = 1.0f / abs(direction.x);
		u = (direction.y * tmp + 1) * 0.5f;
		v = (direction.z * tmp + 1) * 0.5f;
		break;
	case 1:
		tmp = 1.0f / abs(direction.y);
		u = (direction.x * tmp + 1) * 0.5f;
		v = (direction.z * tmp + 1) * 0.5f;
		break;
	case 2:
		tmp = 1.0f / abs(direction.z);
		u = (direction.x * tmp + 1) * 0.5f;
		v = (direction.y * tmp + 1) * 0.5f;
		break;
	
	default:
		u = 0;
		v = 0;
		break;
	}

	if (ret == 0) {
		v = 1.0f - v;
		if (direction.x < 0) {
			position = 1;
		}
		else {
			u = 1.0f - u;
			position = 0;
		}
	}
	else if (ret == 1) {
		v = 1.0f - v;
		//u = 1.0f - u;
		if (direction.y < 0) {
			u = 1.0f - u;
			position = 3;
		}
		else {
			position = 2;
		}
	}
	else {
		if (direction.z < 0) {
			v = 1.0f - v;
			position = 5;
		}
		else {
			v = 1.0f - v;
			u = 1.0f - u;
			position = 4;
		}
	}


	Color3f texel = textures.at(position)->get_texel(u, v);
	return Vector3(texel.r, texel.g, texel.b);
}
