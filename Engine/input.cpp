#include "input.h"

void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));

	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, action != GLFW_RELEASE);
	if (key == GLFW_KEY_W) input->_inputBuffer[static_cast<int>(Key::W)] = action != GLFW_RELEASE;
	if (key == GLFW_KEY_A) input->_inputBuffer[static_cast<int>(Key::A)] = action != GLFW_RELEASE;
	if (key == GLFW_KEY_S) input->_inputBuffer[static_cast<int>(Key::S)] = action != GLFW_RELEASE;
	if (key == GLFW_KEY_D) input->_inputBuffer[static_cast<int>(Key::D)] = action != GLFW_RELEASE;
	if (key == GLFW_KEY_E) input->_inputBuffer[static_cast<int>(Key::E)] = action != GLFW_RELEASE;
	if (key == GLFW_KEY_Q) input->_inputBuffer[static_cast<int>(Key::Q)] = action != GLFW_RELEASE;
}

void Input::cursorCallback(GLFWwindow* window, double xPos, double yPos)
{
	Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));

	input->_xDelta = static_cast<float>(xPos - input->_xPos);
	input->_yDelta = static_cast<float>(yPos - input->_yPos);
	input->_xPos = xPos;
	input->_yPos = yPos;
}

Input::Input(GLFWwindow* window)
{
	_xPos = 0, _yPos = 0;
	_xDelta = 0, _yDelta = 0;

	glfwSetWindowUserPointer(window, this);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (glfwRawMouseMotionSupported())
	{
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	}

	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, cursorCallback);
}

Input::~Input()
{

}