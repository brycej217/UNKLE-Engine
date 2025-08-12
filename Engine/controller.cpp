#include "controller.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

vec3 Controller::projectVector(Camera* camera, const vec3& v)
{
	vec3 flat = v;
	flat.y = camera->transform.position.y;
	return normalize(flat);
}

void Controller::moveCamera(Camera* camera, float deltaTime, vec3 dir)
{
	camera->transform.position += dir * _speed * deltaTime;
}

void Controller::lookCamera(Camera* camera, float deltaTime, float* xDelta, float* yDelta)
{
	float yaw = *xDelta * _sensitivity * deltaTime;
	float pitch = *yDelta * _sensitivity * deltaTime;

	quat yawRot = angleAxis(-yaw, normalize(vec3(0.0f, 1.0f, 0.0f)));
	quat pitchRot = angleAxis(-pitch, normalize(camera->getRight()));

	camera->transform.rotation = normalize(pitchRot * yawRot * camera->transform.rotation);

	*xDelta = 0.0;
	*yDelta = 0.0;
}


void Controller::update(Input* input, Camera* camera, float deltaTime)
{
	vec3 forward = camera->getForward();
	vec3 right = camera->getRight();
	vec3 up = camera->getUp();
	for (int i = 0; i < static_cast<int>(Key::Count); i++)
	{
		if (input->getInputBuffer()[i])
		{
			switch (i)
			{
			case static_cast<int>(Key::W): moveCamera(camera, deltaTime, forward); break;
			case static_cast<int>(Key::A): moveCamera(camera, deltaTime, -right); break;
			case static_cast<int>(Key::S): moveCamera(camera, deltaTime, -forward); break;
			case static_cast<int>(Key::D): moveCamera(camera, deltaTime, right); break;
			case static_cast<int>(Key::E): moveCamera(camera, deltaTime, up); break;
			case static_cast<int>(Key::Q): moveCamera(camera, deltaTime, -up); break;
			default: break;
			}
		}
	}
	lookCamera(camera, deltaTime, input->getXDelta(), input->getYDelta());
}

Controller::Controller()
{

}

Controller::~Controller()
{

}