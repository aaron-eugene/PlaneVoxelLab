///////////////////////////////////////////////////////////////////////////////
// renderer/renderer.h
// ===================
//
// Declares renderer state and basic colored mesh rendering functions.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/gpu_mesh.h"
#include "renderer/shader.h"

#include <glm/ext/matrix_float4x4.hpp>

struct StandardRenderSettings;

/***********************************************************
* Renderer Shader Types
************************************************************/

struct ColorShader
{
	ShaderProgram program = {};

	int modelLocation = -1;
	int viewProjectionLocation = -1;
};

struct StandardShader
{
	ShaderProgram program = {};

	int modelLocation = -1;
	int viewProjectionLocation = -1;

	int shadingModeLocation = -1;
	int lightDirectionLocation = -1;
	int ambientStrengthLocation = -1;
	int diffuseStrengthLocation = -1;
};

/***********************************************************
* Renderer
************************************************************/

struct Renderer
{
	ColorShader colorShader = {};
	StandardShader standardShader = {};
};

/***********************************************************
* Renderer Lifecycle
************************************************************/

bool initializeRenderer(Renderer& renderer);

void shutdownRenderer(Renderer& renderer);

/***********************************************************
* Renderer Frame
************************************************************/

void beginRenderFrame(
	int framebufferWidth,
	int framebufferHeight);

void endRenderFrame();

/***********************************************************
* Renderer Drawing
************************************************************/

void renderColoredMesh(
	const GpuMesh& mesh,
	const ColorShader& shader,
	const glm::mat4& model,
	const glm::mat4& viewProjection);

void renderStandardMesh(
	const GpuMesh& mesh,
	const StandardShader& shader,
	const StandardRenderSettings& settings,
	const glm::mat4& model,
	const glm::mat4& viewProjection);
