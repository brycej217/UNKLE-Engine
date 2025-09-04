#pragma once

#include "renderer.h"
#include "input.h"
#include "controller.h"
#include "scene.h"

#include <glm/glm.hpp>

using namespace std;

class Engine
{
public:
	Engine();

	~Engine();

	void run();

private:
	GLFWwindow* window = nullptr;
	Renderer* renderer;
	SceneManager* sceneManager;
	Camera camera;
	Input* input;
	Controller controller;

	GLFWwindow* initWindow(uint32_t width, uint32_t height, const char* name);

	float calculateDeltaTime(auto* previousTime);
};