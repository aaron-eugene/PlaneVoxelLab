///////////////////////////////////////////////////////////////////////////////
// camera/dev_camera.cpp
// =====================
//
// Implements first-person development camera movement and matrix helpers.
//
///////////////////////////////////////////////////////////////////////////////

#include "camera/dev_camera.h"

#include "camera/camera_math.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cassert>

/***********************************************************
* File-Local Constants
************************************************************/

namespace
{
	constexpr float DEFAULT_VERTICAL_FOV_RADIANS = 1.04719755f; // 60 degrees
	constexpr float MAX_CAMERA_PITCH_RADIANS = 1.55334306f;    // 89 degrees
}

/***********************************************************
* File-Local Helpers
************************************************************/

static glm::vec3 calculateWorldMovementDirection(
	const DevelopmentCamera& camera,
	const DevelopmentCameraControls& controls)
{
	glm::vec3 forward = calculateCameraForward(camera.yawRadians, camera.pitchRadians);
	glm::vec3 worldUp = getCameraWorldUp();
	glm::vec3 right = calculateCameraRight(forward, worldUp);

	glm::vec3 movementDirection = {};
	movementDirection += right * controls.localMovement.x;
	movementDirection += worldUp * controls.localMovement.y;
	movementDirection += forward * controls.localMovement.z;

	const float movementLength = glm::length(movementDirection);
	if (movementLength > 0.0f)
	{
		movementDirection /= movementLength;
	}

	return movementDirection;
}

/***********************************************************
* Development Camera Interface
************************************************************/

void initializeDevelopmentCamera(DevelopmentCamera& camera)
{
	camera = {};

	camera.position = glm::vec3(0.0f, 0.0f, 3.0f);

	camera.yawRadians = 0.0f;
	camera.pitchRadians = 0.0f;

	camera.moveSpeed = 4.0f;
	camera.speedBoostMultiplier = 4.0f;
	camera.lookSensitivity = 0.0025f;

	camera.verticalFovRadians = DEFAULT_VERTICAL_FOV_RADIANS;
	camera.nearPlane = 0.1f;
	camera.farPlane = 1000.0f;
}

void updateDevelopmentCamera(
	DevelopmentCamera& camera,
	const DevelopmentCameraControls& controls,
	float deltaTime)
{
	assert(deltaTime >= 0.0f);
	assert(camera.moveSpeed >= 0.0f);
	assert(camera.speedBoostMultiplier >= 0.0f);
	assert(camera.lookSensitivity >= 0.0f);

	camera.yawRadians += controls.lookDelta.x * camera.lookSensitivity;
	camera.pitchRadians += controls.lookDelta.y * camera.lookSensitivity;

	camera.pitchRadians = glm::clamp(
		camera.pitchRadians,
		-MAX_CAMERA_PITCH_RADIANS,
		MAX_CAMERA_PITCH_RADIANS);

	glm::vec3 movementDirection = calculateWorldMovementDirection(camera, controls);

	float currentMoveSpeed = camera.moveSpeed;
	if (controls.speedBoost)
	{
		currentMoveSpeed *= camera.speedBoostMultiplier;
	}

	camera.position += movementDirection * currentMoveSpeed * deltaTime;
}

glm::mat4 buildDevelopmentCameraViewMatrix(const DevelopmentCamera& camera)
{
	return buildCameraViewMatrix(
		camera.position,
		camera.yawRadians,
		camera.pitchRadians);
}

glm::mat4 buildDevelopmentCameraProjectionMatrix(
	const DevelopmentCamera& camera,
	float aspectRatio)
{
	assert(aspectRatio > 0.0f);
	assert(camera.verticalFovRadians > 0.0f);
	assert(camera.nearPlane > 0.0f);
	assert(camera.farPlane > camera.nearPlane);

	return buildPerspectiveProjectionMatrix(
		camera.verticalFovRadians,
		aspectRatio,
		camera.nearPlane,
		camera.farPlane);
}

glm::mat4 buildDevelopmentCameraViewProjectionMatrix(
	const DevelopmentCamera& camera,
	float aspectRatio)
{
	glm::mat4 projection = buildDevelopmentCameraProjectionMatrix(camera, aspectRatio);
	glm::mat4 view = buildDevelopmentCameraViewMatrix(camera);

	return projection * view;
}
