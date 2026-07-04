///////////////////////////////////////////////////////////////////////////////
// camera/camera_math.h
// ====================
//
// Declares camera math helpers for building camera direction vectors and view /
// projection matrices.
// 
// Notes:
// - Yaw/pitch are in radians
// - Yaw = 0, pitch = 0 looks down negative Z.
// - Positive yaw turns right toward positive X.
// - Positive pitch looks upward.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/glm.hpp>

/***********************************************************
* Camera Math Interface
************************************************************/

glm::vec3 getCameraWorldUp();

glm::vec3 calculateCameraForward(float yawRadians, float pitchRadians);

glm::vec3 calculateCameraRight(
	const glm::vec3& forward,
	const glm::vec3& worldUp);

glm::vec3 calculateCameraUp(
	const glm::vec3& right,
	const glm::vec3& forward);

glm::mat4 buildCameraViewMatrix(
	const glm::vec3& position,
	float yawRadians,
	float pitchRadians);

glm::mat4 buildPerspectiveProjectionMatrix(
	float verticalFovRadians,
	float aspectRatio,
	float nearPlane,
	float farPlane);
