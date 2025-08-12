#pragma once

#include "renderer.h"
#include "input.h"
#include <glm/glm.hpp>

using namespace glm;

class Controller
{
public:
	Controller();

	~Controller();

	void update(Input* input, Camera* camera, float deltaTime);

private:
	float _speed = 25.0f;
	float _sensitivity = 0.7f;

	void moveCamera(Camera* camera, float deltaTime, vec3 dir);

	void lookCamera(Camera* camera, float deltaTime, float* xDelta, float* yDelta);

	vec3 projectVector(Camera* camera, const vec3& v);
};