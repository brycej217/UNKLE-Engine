#pragma once

#include <GLFW/glfw3.h>

enum class Key
{
	W,
	A,
	S,
	D,
	E,
	Q,
	Count
};

class Input
{
public:
	Input(GLFWwindow* window);

	~Input();

	bool* getInputBuffer() {return _inputBuffer; }

	float* getXDelta() { return &_xDelta; }

	float* getYDelta() { return &_yDelta; }

private:
	bool _inputBuffer[static_cast<int>(Key::Count)] = {};
	double _xPos, _yPos;
	float _xDelta, _yDelta;

	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	static void cursorCallback(GLFWwindow* window, double xpos, double ypos);

};