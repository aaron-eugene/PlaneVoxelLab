///////////////////////////////////////////////////////////////////////////////
// renderer/renderer.cpp
// =====================
//
// Implements renderer initialization, frame setup, and basic mesh rendering.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/renderer.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>

/***********************************************************
* Shader Asset Paths
************************************************************/

static constexpr const char* COLOR_VERTEX_SHADER_PATH =
	"assets/shaders/color.vert";

static constexpr const char* COLOR_FRAGMENT_SHADER_PATH =
	"assets/shaders/color.frag";

/***********************************************************
* Shader Uniform Names
************************************************************/

static constexpr const char* UNIFORM_MODEL = "uModel";
static constexpr const char* UNIFORM_VIEW_PROJECTION = "uViewProjection";

/***********************************************************
* Internal Helpers
************************************************************/

static GLenum getOpenGlPrimitiveType(GpuPrimitiveType primitiveType)
{
	switch (primitiveType)
	{
	case GpuPrimitiveType::Triangles:
	{
		return GL_TRIANGLES;
	}

	case GpuPrimitiveType::Lines:
	{
		return GL_LINES;
	}

	default:
	{
		assert(false);
		return GL_TRIANGLES;
	}
	}
}

static void shutdownColorShader(ColorShader& shader)
{
	destroyShaderProgram(shader.program);

	shader = {};
}

static bool initializeColorShader(ColorShader& shader)
{
	assert(shader.program.handle == 0);
	assert(shader.modelLocation == -1);
	assert(shader.viewProjectionLocation == -1);

	ShaderSource shaderSource = {};

	if (!loadShaderSource(
		shaderSource,
		COLOR_VERTEX_SHADER_PATH,
		COLOR_FRAGMENT_SHADER_PATH))
	{
		return false;
	}

	const bool shaderCreated =
		createShaderProgram(
			shader.program,
			shaderSource);

	destroyShaderSource(shaderSource);

	if (!shaderCreated)
	{
		return false;
	}

	shader.modelLocation =
		getShaderUniformLocation(shader.program, UNIFORM_MODEL);

	shader.viewProjectionLocation =
		getShaderUniformLocation(shader.program, UNIFORM_VIEW_PROJECTION);

	if (shader.modelLocation < 0 ||
		shader.viewProjectionLocation < 0)
	{
		shutdownColorShader(shader);
		return false;
	}

	return true;
}

/***********************************************************
* Renderer Lifecycle
************************************************************/

bool initializeRenderer(Renderer& renderer)
{
	assert(renderer.colorShader.program.handle == 0);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (!initializeColorShader(renderer.colorShader))
	{
		shutdownRenderer(renderer);
		return false;
	}

	return true;
}

void shutdownRenderer(Renderer& renderer)
{
	shutdownColorShader(renderer.colorShader);

	renderer = {};
}

/***********************************************************
* Renderer Frame
************************************************************/

void beginRenderFrame(
	int framebufferWidth,
	int framebufferHeight)
{
	assert(framebufferWidth > 0);
	assert(framebufferHeight > 0);

	glViewport(0, 0, framebufferWidth, framebufferHeight);
	glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void endRenderFrame()
{
}

/***********************************************************
* Renderer Drawing
************************************************************/

void renderMesh(
	const GpuMesh& mesh,
	const ColorShader& shader,
	const glm::mat4& model,
	const glm::mat4& viewProjection)
{
	assert(mesh.vertexArray != 0);
	assert(mesh.indexCount > 0);
	assert(shader.program.handle != 0);
	assert(shader.modelLocation >= 0);
	assert(shader.viewProjectionLocation >= 0);

	glUseProgram(shader.program.handle);

	glUniformMatrix4fv(
		shader.modelLocation,
		1,
		GL_FALSE,
		glm::value_ptr(model));

	glUniformMatrix4fv(
		shader.viewProjectionLocation,
		1,
		GL_FALSE,
		glm::value_ptr(viewProjection));

	glBindVertexArray(mesh.vertexArray);

	glDrawElements(
		getOpenGlPrimitiveType(mesh.primitiveType),
		mesh.indexCount,
		GL_UNSIGNED_INT,
		nullptr);

	glBindVertexArray(0);

	glUseProgram(0);
}
