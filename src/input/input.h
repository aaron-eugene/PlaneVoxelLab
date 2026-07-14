///////////////////////////////////////////////////////////////////////////////
// input/input.h
// =============
//
// Declares application input state and helper functions for polling keyboard
// and mouse input.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

struct GLFWwindow;

/***********************************************************
* Input Action
************************************************************/

enum class InputAction : uint8_t
{
	MoveForward = 0,
	MoveBackward,
	MoveLeft,
	MoveRight,
	MoveUp,
	MoveDown,

	SpeedBoost,
	Quit,

	ToggleCursorLock,

	ToggleSurfaceReference,
	ToggleActiveExperiment,
	ToggleChunkWireframes,
	RebuildMeshes,

	Count
};

/***********************************************************
* Input State
************************************************************/

struct InputState
{
	bool actions[static_cast<uint32_t>(InputAction::Count)] = {};
	bool previousActions[static_cast<uint32_t>(InputAction::Count)] = {};

	double mouseX = 0.0;
	double mouseY = 0.0;

	double previousMouseX = 0.0;
	double previousMouseY = 0.0;

	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;

	bool mouseInitialized = false;
};

/***********************************************************
* Input Interface
************************************************************/

void initializeInput(InputState& input, GLFWwindow* window);

void updateInput(InputState& input, GLFWwindow* window);

bool isActionDown(const InputState& input, InputAction action);

bool wasActionPressed(const InputState& input, InputAction action);

bool wasActionReleased(const InputState& input, InputAction action);
