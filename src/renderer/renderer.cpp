///////////////////////////////////////////////////////////////////////////////
// renderer/renderer.cpp
// =====================
//
// Implements renderer initialization, frame setup, and basic mesh rendering.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/renderer.h"

#include "renderer/gpu_mesh.h"
#include "renderer/shader.h"
#include "renderer/standard_render_settings.h"
#include "renderer/texture_2d.h"

#include <glad/glad.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>

/***********************************************************
* Local Constants
************************************************************/

static constexpr float LIGHT_DIRECTION_EPSILON = 0.00001f;

/***********************************************************
* Shader Asset Paths
************************************************************/

static constexpr const char* COLOR_VERTEX_SHADER_PATH =
	"assets/shaders/color.vert";

static constexpr const char* COLOR_FRAGMENT_SHADER_PATH =
	"assets/shaders/color.frag";

static constexpr const char* STANDARD_VERTEX_SHADER_PATH =
	"assets/shaders/standard.vert";

static constexpr const char* STANDARD_FRAGMENT_SHADER_PATH =
	"assets/shaders/standard.frag";

/***********************************************************
* Shader Uniform Names
************************************************************/

static constexpr const char* UNIFORM_MODEL = "uModel";
static constexpr const char* UNIFORM_VIEW_PROJECTION = "uViewProjection";
static constexpr const char* UNIFORM_SHADING_MODE = "uShadingMode";
static constexpr const char* UNIFORM_LIGHT_DIRECTION = "uLightDirection";
static constexpr const char* UNIFORM_AMBIENT_STRENGTH = "uAmbientStrength";
static constexpr const char* UNIFORM_DIFFUSE_STRENGTH = "uDiffuseStrength";
static constexpr const char* UNIFORM_TEXTURE = "uTexture";

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

static void shutdownStandardShader(StandardShader& shader)
{
	destroyShaderProgram(shader.program);

	shader = {};
}

static bool initializeStandardShader(StandardShader& shader)
{
	assert(shader.program.handle == 0);

	assert(shader.modelLocation == -1);
	assert(shader.viewProjectionLocation == -1);

	assert(shader.shadingModeLocation == -1);
	assert(shader.lightDirectionLocation == -1);
	assert(shader.ambientStrengthLocation == -1);
	assert(shader.diffuseStrengthLocation == -1);
	assert(shader.textureLocation == -1);

	ShaderSource shaderSource = {};

	if (!loadShaderSource(
		shaderSource,
		STANDARD_VERTEX_SHADER_PATH,
		STANDARD_FRAGMENT_SHADER_PATH))
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
		getShaderUniformLocation(
			shader.program,
			UNIFORM_MODEL);

	shader.viewProjectionLocation =
		getShaderUniformLocation(
			shader.program,
			UNIFORM_VIEW_PROJECTION);

	shader.shadingModeLocation =
		getShaderUniformLocation(
			shader.program,
			UNIFORM_SHADING_MODE);

	shader.lightDirectionLocation =
		getShaderUniformLocation(
			shader.program,
			UNIFORM_LIGHT_DIRECTION);

	shader.ambientStrengthLocation =
		getShaderUniformLocation(
			shader.program,
			UNIFORM_AMBIENT_STRENGTH);

	shader.diffuseStrengthLocation =
		getShaderUniformLocation(
			shader.program,
			UNIFORM_DIFFUSE_STRENGTH);

	shader.textureLocation =
		getShaderUniformLocation(
			shader.program,
			UNIFORM_TEXTURE);

	if (shader.modelLocation < 0 ||
		shader.viewProjectionLocation < 0 ||
		shader.shadingModeLocation < 0 ||
		shader.lightDirectionLocation < 0 ||
		shader.ambientStrengthLocation < 0 ||
		shader.diffuseStrengthLocation < 0 ||
		shader.textureLocation < 0)
	{
		shutdownStandardShader(shader);
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
	assert(renderer.standardShader.program.handle == 0);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	glBlendFunc(
		GL_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA);

	if (!initializeColorShader(
		renderer.colorShader))
	{
		shutdownRenderer(renderer);
		return false;
	}

	if (!initializeStandardShader(
		renderer.standardShader))
	{
		shutdownRenderer(renderer);
		return false;
	}

	return true;
}

void shutdownRenderer(Renderer& renderer)
{
	shutdownStandardShader(
		renderer.standardShader);

	shutdownColorShader(
		renderer.colorShader);

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

static void drawGpuMesh(
	const GpuMesh& mesh)
{
	assert(mesh.vertexArray != 0);
	assert(mesh.indexCount > 0);

	glBindVertexArray(mesh.vertexArray);

	glDrawElements(
		getOpenGlPrimitiveType(mesh.primitiveType),
		mesh.indexCount,
		GL_UNSIGNED_INT,
		nullptr);

	glBindVertexArray(0);
}

void renderColoredMesh(
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

	drawGpuMesh(mesh);

	glUseProgram(0);
}

void renderStandardMesh(
	const GpuMesh& mesh,
	const StandardShader& shader,
	const StandardRenderSettings& settings,
	const Texture2D& texture,
	const glm::mat4& model,
	const glm::mat4& viewProjection)
{
	assert(mesh.vertexArray != 0);
	assert(mesh.indexCount > 0);

	assert(shader.program.handle != 0);
	assert(shader.modelLocation >= 0);
	assert(shader.viewProjectionLocation >= 0);
	assert(shader.shadingModeLocation >= 0);
	assert(shader.lightDirectionLocation >= 0);
	assert(shader.ambientStrengthLocation >= 0);
	assert(shader.diffuseStrengthLocation >= 0);
	assert(shader.textureLocation >= 0);

	assert(texture.handle != 0);
	assert(texture.width > 0);
	assert(texture.height > 0);

	assert(settings.ambientStrength >= 0.0f);
	assert(settings.diffuseStrength >= 0.0f);

	const float lightDirectionLengthSquared =
		glm::dot(
			settings.lightDirection,
			settings.lightDirection);

	assert(
		lightDirectionLengthSquared >
		LIGHT_DIRECTION_EPSILON *
		LIGHT_DIRECTION_EPSILON);

	constexpr GLuint textureUnit = 0;

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

	glUniform1i(
		shader.shadingModeLocation,
		static_cast<int>(
			settings.shadingMode));

	glUniform3fv(
		shader.lightDirectionLocation,
		1,
		glm::value_ptr(
			settings.lightDirection));

	glUniform1f(
		shader.ambientStrengthLocation,
		settings.ambientStrength);

	glUniform1f(
		shader.diffuseStrengthLocation,
		settings.diffuseStrength);

	glBindTextureUnit(
		textureUnit,
		texture.handle);

	glUniform1i(
		shader.textureLocation,
		static_cast<GLint>(
			textureUnit));

	drawGpuMesh(mesh);

	glBindTextureUnit(
		textureUnit,
		0);

	glUseProgram(0);
}
