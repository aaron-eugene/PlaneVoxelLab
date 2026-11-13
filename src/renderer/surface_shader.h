
///////////////////////////////////////////////////////////////////////////////
// renderer/surface_shader.h
// =========================
//
// Declares a rendering path for surfaces with geometric and continuous
// normals.
//
// Supports normal blending, normal visualization, texturing, and basic
// directional lighting. Normal generation is handled separately.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/shader.h"

#include <glm/ext/matrix_float4x4.hpp>

struct GpuMesh;
struct StandardRenderSettings;
struct Texture2D;

/***********************************************************
* Surface Shader Types
************************************************************/

struct SurfaceShader
{
	ShaderProgram program = {};

	int modelLocation = -1;
	int viewProjectionLocation = -1;

	int textureLocation = -1;
	int lightDirectionLocation = -1;
	int ambientStrengthLocation = -1;
	int diffuseStrengthLocation = -1;

	int normalBlendLocation = -1;
	int visualizeNormalsLocation = -1;
};

struct SurfaceRenderSettings
{
	float normalBlend = 0.0f;
	bool visualizeNormals = false;
};

/***********************************************************
* Surface Shader Lifecycle
************************************************************/

bool initializeSurfaceShader(
	SurfaceShader& shader);

void shutdownSurfaceShader(
	SurfaceShader& shader);

/***********************************************************
* Surface Rendering
************************************************************/

void renderSurfaceMesh(
	const SurfaceShader& shader,
	const GpuMesh& mesh,
	const SurfaceRenderSettings& surfaceSettings,
	const StandardRenderSettings& renderSettings,
	const Texture2D& texture,
	const glm::mat4& model,
	const glm::mat4& viewProjection);
