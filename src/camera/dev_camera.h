///////////////////////////////////////////////////////////////////////////////
// camera/dev_camera.h
// ===================
//
// Declares a first-person development camera for debug, tooling, and free-camera
// navigation.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/glm.hpp>

/***********************************************************
* Development Camera Controls
************************************************************/

struct DevelopmentCameraControls
{
	// Local-space movement intent:
	// +X = right, -X = left
	// +Y = up,    -Y = down
	// +Z = forward, -Z = backward
	glm::vec3 localMovement = {};

	// Look delta in screen/input space:
	// +X = look right
	// +Y = look up
	glm::vec2 lookDelta = {};

	bool speedBoost = false;
};

/***********************************************************
* Development Camera
************************************************************/

struct DevelopmentCamera
{
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);

	float yawRadians = 0.0f;
	float pitchRadians = 0.0f;

	float moveSpeed = 4.0f;
	float speedBoostMultiplier = 4.0f;
	float lookSensitivity = 0.0025f;

	float verticalFovRadians = 1.04719755f; // 60 degrees
	float nearPlane = 0.1f;
	float farPlane = 1000.0f;
};

/***********************************************************
* Development Camera Interface
************************************************************/

void initializeDevelopmentCamera(DevelopmentCamera& camera);

void updateDevelopmentCamera(
	DevelopmentCamera& camera,
	const DevelopmentCameraControls& controls,
	float deltaTime);

glm::mat4 buildDevelopmentCameraViewMatrix(const DevelopmentCamera& camera);

glm::mat4 buildDevelopmentCameraProjectionMatrix(
	const DevelopmentCamera& camera,
	float aspectRatio);

glm::mat4 buildDevelopmentCameraViewProjectionMatrix(
	const DevelopmentCamera& camera,
	float aspectRatio);
