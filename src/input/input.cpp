///////////////////////////////////////////////////////////////////////////////
// input/input.cpp
// ===============
//
// Implements GLFW-backed input polling helpers.
//
///////////////////////////////////////////////////////////////////////////////

#include "input/input.h"

#include <glfw/glfw3.h>

#include <cassert>
#include <cstdint>

/***********************************************************
* File-Local Helpers
************************************************************/

static uint32_t getInputActionIndex(InputAction action)
{
	uint32_t index = static_cast<uint32_t>(action);

	assert(index < static_cast<uint32_t>(InputAction::Count));

	return index;
}

static int getGlfwKeyForAction(InputAction action)
{
	switch (action)
	{
	case InputAction::MoveForward:
		return GLFW_KEY_W;

	case InputAction::MoveBackward:
		return GLFW_KEY_S;

	case InputAction::MoveLeft:
		return GLFW_KEY_A;

	case InputAction::MoveRight:
		return GLFW_KEY_D;

	case InputAction::MoveUp:
		return GLFW_KEY_E;

	case InputAction::MoveDown:
		return GLFW_KEY_Q;

	case InputAction::SpeedBoost:
		return GLFW_KEY_LEFT_SHIFT;

	case InputAction::Quit:
		return GLFW_KEY_ESCAPE;

	case InputAction::ToggleCursorLock:
		return GLFW_KEY_TAB;

	case InputAction::ToggleSurfaceReference:
		return GLFW_KEY_1;

	case InputAction::ToggleActiveExperiment:
		return GLFW_KEY_2;

	case InputAction::ToggleVoxelGrid:
		return GLFW_KEY_3;

	case InputAction::RebuildMeshes:
		return GLFW_KEY_R;

	default:
		assert(false);
		return GLFW_KEY_UNKNOWN;
	}
}

static void updateActionState(InputState& input, GLFWwindow* window)
{
	for (uint32_t actionIndex = 0;
		actionIndex < static_cast<uint32_t>(InputAction::Count);
		++actionIndex)
	{
		input.previousActions[actionIndex] = input.actions[actionIndex];

		InputAction action = static_cast<InputAction>(actionIndex);
		int glfwKey = getGlfwKeyForAction(action);

		input.actions[actionIndex] = glfwGetKey(window, glfwKey) == GLFW_PRESS;
	}
}

static void updateMouseState(InputState& input, GLFWwindow* window)
{
	input.previousMouseX = input.mouseX;
	input.previousMouseY = input.mouseY;

	glfwGetCursorPos(
		window,
		&input.mouseX,
		&input.mouseY);

	if (!input.mouseInitialized)
	{
		input.previousMouseX = input.mouseX;
		input.previousMouseY = input.mouseY;
		input.mouseInitialized = true;
	}

	input.mouseDeltaX = static_cast<float>(input.mouseX - input.previousMouseX);
	input.mouseDeltaY = static_cast<float>(input.previousMouseY - input.mouseY);
}

/***********************************************************
* Input Interface
************************************************************/

void initializeInput(InputState& input, GLFWwindow* window)
{
	assert(window != nullptr);

	input = {};

	glfwGetCursorPos(
		window,
		&input.mouseX,
		&input.mouseY);

	input.previousMouseX = input.mouseX;
	input.previousMouseY = input.mouseY;
	input.mouseInitialized = true;

	updateActionState(input, window);
}

void updateInput(InputState& input, GLFWwindow* window)
{
	assert(window != nullptr);

	updateActionState(input, window);
	updateMouseState(input, window);
}

bool isActionDown(const InputState& input, InputAction action)
{
	uint32_t actionIndex = getInputActionIndex(action);

	return input.actions[actionIndex];
}

bool wasActionPressed(const InputState& input, InputAction action)
{
	uint32_t actionIndex = getInputActionIndex(action);

	return input.actions[actionIndex] && !input.previousActions[actionIndex];
}

bool wasActionReleased(const InputState& input, InputAction action)
{
	uint32_t actionIndex = getInputActionIndex(action);

	return !input.actions[actionIndex] && input.previousActions[actionIndex];
}
