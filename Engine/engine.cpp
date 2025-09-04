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

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float deltaTime = calculateDeltaTime(&previousTime);

		controller.update(input, &camera, deltaTime);

		if (input->getInputBuffer()[static_cast<int>(Key::G)])
		{
			renderer->switchPipeline();
			input->getInputBuffer()[static_cast<int>(Key::G)] = false;
		}

		renderer->render(camera, deltaTime);
	}
}

Engine::Engine()
{
	this->window = initWindow(WIDTH, HEIGHT, "Engine");
	this->renderer = new Renderer(window, WIDTH, HEIGHT);
	this->sceneManager = new SceneManager(this->renderer);
	this->input = new Input(this->window);

	camera =
	{
		.transform =
		{
			.position = vec3(0.0f, 0.5f, 3.0f),
			.rotation = quat(),
		},
	};

	sceneManager->loadScene("scenes/scene.fbx");

	renderer->createPipeline();
}

Engine::~Engine()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}