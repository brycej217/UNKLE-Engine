#define GLM_ENABLE_EXPERIMENTAL

#include "engine.h"
#include "input.h"

#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#define WIDTH 1920
#define HEIGHT 1080

using namespace std;
using namespace chrono;
using namespace glm;

GLFWwindow* Engine::initWindow(uint32_t width, uint32_t height, const char* name)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	GLFWwindow* window = glfwCreateWindow(width, height, name, nullptr, nullptr);

	return window;
}

float Engine::calculateDeltaTime(auto* previousTime)
{
	auto currentTime = high_resolution_clock::now();
	float deltaTime = duration<float, seconds::period>(currentTime - *previousTime).count();
	*previousTime = currentTime;

	return deltaTime;
}

void Engine::run()
{
	auto previousTime = high_resolution_clock::now();

	while (!glfwWindowShouldClose(_window))
	{
		glfwPollEvents();

		float deltaTime = calculateDeltaTime(&previousTime);

		_controller.update(&_input, &_camera, deltaTime);

		if (_input.getInputBuffer()[static_cast<int>(Key::G)])
		{
			_renderer.switchPipeline();
			_input.getInputBuffer()[static_cast<int>(Key::G)] = false;
		}

		_renderer.render(_camera, deltaTime);
	}
}

Engine::Engine()
	: _window(initWindow(WIDTH, HEIGHT, "Engine"))
	, _renderer(_window, WIDTH, HEIGHT)
	, _sceneManager(&_renderer)
	, _input(_window)
{
	_camera =
	{
		.transform =
		{
			.position = vec3(0.0f, 0.5f, 3.0f),
			.rotation = quat(),
		},
	};
	//_camera.transform.rotation = angleAxis(radians(-90.0f), _camera.getRight());

	_sceneManager.loadScene("scenes/scene.fbx");
	_renderer.createPipeline();
}

Engine::~Engine()
{
	glfwDestroyWindow(_window);
	glfwTerminate();
}