
///////////////////////////////////////////////////////////////////////////////
// renderer/surface_shader.cpp
// ===========================
//
// Implements rendering of surfaces with geometric and continuous normals.
//
// The shader blends the two normal sources for lighting and supports
// direct visualization of the resulting normals.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/surface_shader.h"

#include "renderer/gpu_mesh.h"
#include "renderer/standard_render_settings.h"
#include "renderer/texture_2d.h"

#include <glad/glad.h>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>

/***********************************************************
* Surface Shader Lifecycle
************************************************************/

bool initializeSurfaceShader(
	SurfaceShader& shader)
{
	assert(shader.program.handle == 0);

	ShaderSource source = {};

	if (!loadShaderSource(
		source,
		"assets/shaders/surface.vert",
		"assets/shaders/surface.frag"))
	{
		return false;
	}

	const bool created =
		createShaderProgram(
			shader.program,
			source);

	clearShaderSource(source);

	if (!created)
	{
		return false;
	}

	shader.modelLocation =
		getShaderUniformLocation(
			shader.program,
			"uModel");

	shader.viewProjectionLocation =
		getShaderUniformLocation(
			shader.program,
			"uViewProjection");

	shader.textureLocation =
		getShaderUniformLocation(
			shader.program,
			"uTexture");

	shader.lightDirectionLocation =
		getShaderUniformLocation(
			shader.program,
			"uLightDirection");

	shader.ambientStrengthLocation =
		getShaderUniformLocation(
			shader.program,
			"uAmbientStrength");

	shader.diffuseStrengthLocation =
		getShaderUniformLocation(
			shader.program,
			"uDiffuseStrength");

	shader.normalBlendLocation =
		getShaderUniformLocation(
			shader.program,
			"uNormalBlend");

	shader.visualizeNormalsLocation =
		getShaderUniformLocation(
			shader.program,
			"uVisualizeNormals");

	if (shader.modelLocation < 0 ||
		shader.viewProjectionLocation < 0 ||
		shader.textureLocation < 0 ||
		shader.lightDirectionLocation < 0 ||
		shader.ambientStrengthLocation < 0 ||
		shader.diffuseStrengthLocation < 0 ||
		shader.normalBlendLocation < 0 ||
		shader.visualizeNormalsLocation < 0)
	{
		shutdownSurfaceShader(
			shader);

		return false;
	}

	return true;
}

void shutdownSurfaceShader(
	SurfaceShader& shader)
{
	destroyShaderProgram(
		shader.program);

	shader = {};
}

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
	const glm::mat4& viewProjection)
{
	assert(shader.program.handle != 0);

	assert(mesh.vertexArray != 0);
	assert(mesh.indexCount > 0);
	assert(mesh.primitiveType == GpuPrimitiveType::Triangles);

	assert(texture.handle != 0);

	assert(renderSettings.ambientStrength >= 0.0f);
	assert(renderSettings.diffuseStrength >= 0.0f);

	assert(surfaceSettings.normalBlend >= 0.0f);
	assert(surfaceSettings.normalBlend <= 1.0f);

	constexpr GLuint textureUnit = 0;

	glUseProgram(
		shader.program.handle);

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

	glUniform3fv(
		shader.lightDirectionLocation,
		1,
		glm::value_ptr(
			renderSettings.lightDirection));

	glUniform1f(
		shader.ambientStrengthLocation,
		renderSettings.ambientStrength);

	glUniform1f(
		shader.diffuseStrengthLocation,
		renderSettings.diffuseStrength);

	glUniform1f(
		shader.normalBlendLocation,
		surfaceSettings.normalBlend);

	glUniform1i(
		shader.visualizeNormalsLocation,
		surfaceSettings.visualizeNormals ? 1 : 0);

	glBindTextureUnit(
		textureUnit,
		texture.handle);

	glUniform1i(
		shader.textureLocation,
		static_cast<GLint>(
			textureUnit));

	glBindVertexArray(
		mesh.vertexArray);

	glDrawElements(
		GL_TRIANGLES,
		mesh.indexCount,
		GL_UNSIGNED_INT,
		nullptr);

	glBindVertexArray(0);

	glBindTextureUnit(
		textureUnit,
		0);

	glUseProgram(0);
}
