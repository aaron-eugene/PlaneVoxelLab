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

#include <glm/mat4x4.hpp>

/***********************************************************
* Renderer Shader Types
************************************************************/

struct ColorShader
{
	ShaderProgram program = {};

	int modelLocation = -1;
	int viewProjectionLocation = -1;
};

/***********************************************************
* Renderer Types
************************************************************/

struct Renderer
{
	ColorShader colorShader = {};
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

void renderMesh(
	const GpuMesh& mesh,
	const ColorShader& shader,
	const glm::mat4& model,
	const glm::mat4& viewProjection);
