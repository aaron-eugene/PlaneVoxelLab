///////////////////////////////////////////////////////////////////////////////
// camera/camera_math.cpp
// ======================
//
// Implements camera math helpers for direction vectors and view / projection
// matrices.
//
///////////////////////////////////////////////////////////////////////////////

#include "camera/camera_math.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cassert>
#include <cmath>

/***********************************************************
* Camera Math Interface
************************************************************/

glm::vec3 getCameraWorldUp()
{
	return glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 calculateCameraForward(float yawRadians, float pitchRadians)
{
	float cosPitch = std::cos(pitchRadians);

	glm::vec3 forward = {};
	forward.x = std::sin(yawRadians) * cosPitch;
	forward.y = std::sin(pitchRadians);
	forward.z = -std::cos(yawRadians) * cosPitch;

	return glm::normalize(forward);
}

glm::vec3 calculateCameraRight(
	const glm::vec3& forward,
	const glm::vec3& worldUp)
{
	assert(glm::length(forward) > 0.0f);
	assert(glm::length(worldUp) > 0.0f);

	return glm::normalize(glm::cross(forward, worldUp));
}

glm::vec3 calculateCameraUp(
	const glm::vec3& right,
	const glm::vec3& forward)
{
	assert(glm::length(right) > 0.0f);
	assert(glm::length(forward) > 0.0f);

	return glm::normalize(glm::cross(right, forward));
}

glm::mat4 buildCameraViewMatrix(
	const glm::vec3& position,
	float yawRadians,
	float pitchRadians)
{
	glm::vec3 worldUp = getCameraWorldUp();
	glm::vec3 forward = calculateCameraForward(yawRadians, pitchRadians);
	glm::vec3 right = calculateCameraRight(forward, worldUp);
	glm::vec3 up = calculateCameraUp(right, forward);

	return glm::lookAt(
		position,
		position + forward,
		up);
}

glm::mat4 buildPerspectiveProjectionMatrix(
	float verticalFovRadians,
	float aspectRatio,
	float nearPlane,
	float farPlane)
{
	assert(verticalFovRadians > 0.0f);
	assert(aspectRatio > 0.0f);
	assert(nearPlane > 0.0f);
	assert(farPlane > nearPlane);

	return glm::perspective(
		verticalFovRadians,
		aspectRatio,
		nearPlane,
		farPlane);
}
