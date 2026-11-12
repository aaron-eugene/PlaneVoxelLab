
///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_surface_normals.cpp
// ================================
//
// Implements GPU surface-normal buffer management and compute-based
// generation of continuous normals from procedural density fields.
//
// The initial implementation supports sphere density fields.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/gpu_surface_normals.h"

#include "fields/field_generators.h"
#include "renderer/gpu_mesh.h"
#include "renderer/shader.h"

#include <glad/glad.h>

#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

/***********************************************************
* File-Local Constants
************************************************************/

static constexpr uint32_t NORMAL_COMPUTE_WORKGROUP_SIZE = 128;

static constexpr const char* NORMAL_COMPUTE_SHADER_PATH =
"assets/shaders/fragment_cleanup_normals.comp";

static constexpr const char* WORLD_POSITION_SHADER_PATH =
"assets/shaders/spatial/world_position.glsl";

static constexpr const char* SPHERE_DENSITY_SHADER_PATH =
"assets/shaders/fields/sphere_density.glsl";

/***********************************************************
* Compute Source Helpers
************************************************************/

static bool loadGpuSurfaceNormalComputeSource(
	std::string& computeSource,
	const char* fieldShaderPath)
{
	assert(computeSource.empty());
	assert(fieldShaderPath != nullptr);

	if (!loadShaderSourceFile(
		computeSource,
		NORMAL_COMPUTE_SHADER_PATH))
	{
		return false;
	}

	std::string spatialSource = {};

	if (!loadShaderSourceFile(
		spatialSource,
		WORLD_POSITION_SHADER_PATH))
	{
		return false;
	}

	if (!insertShaderSource(
		computeSource,
		"// @WORLD_POSITION_IMPLEMENTATION@",
		spatialSource))
	{
		return false;
	}

	std::string fieldSource = {};

	if (!loadShaderSourceFile(
		fieldSource,
		fieldShaderPath))
	{
		return false;
	}

	if (!insertShaderSource(
		computeSource,
		"// @DENSITY_FIELD_IMPLEMENTATION@",
		fieldSource))
	{
		return false;
	}

	return true;
}

/***********************************************************
* GPU Surface Normal Generator Lifecycle
************************************************************/

bool initializeGpuSurfaceNormalGenerator(
	GpuSurfaceNormalGenerator& generator)
{
	assert(generator.program.handle == 0);

	std::string computeSource = {};

	if (!loadGpuSurfaceNormalComputeSource(
		computeSource,
		SPHERE_DENSITY_SHADER_PATH))
	{
		return false;
	}

	if (!createComputeShaderProgram(
		generator.program,
		computeSource.c_str()))
	{
		return false;
	}

	generator.vertexCountLocation =
		getShaderUniformLocation(
			generator.program,
			"uVertexCount");

	generator.meshChunkLocation =
		getShaderUniformLocation(
			generator.program,
			"uMeshChunk");

	generator.sphereCenterChunkLocation =
		getShaderUniformLocation(
			generator.program,
			"uSphereCenterChunk");

	generator.sphereCenterLocalLocation =
		getShaderUniformLocation(
			generator.program,
			"uSphereCenterLocal");

	generator.sphereRadiusLocation =
		getShaderUniformLocation(
			generator.program,
			"uSphereRadius");

	if (generator.vertexCountLocation < 0 ||
		generator.meshChunkLocation < 0 ||
		generator.sphereCenterChunkLocation < 0 ||
		generator.sphereCenterLocalLocation < 0)
	{
		shutdownGpuSurfaceNormalGenerator(
			generator);

		return false;
	}

	return true;
}

void shutdownGpuSurfaceNormalGenerator(
	GpuSurfaceNormalGenerator& generator)
{
	destroyShaderProgram(
		generator.program);

	generator = {};
}

/***********************************************************
* GPU Surface Normal Buffer Lifecycle
************************************************************/

bool createGpuSurfaceNormalBuffer(
	GpuSurfaceNormalBuffer& buffer,
	uint32_t normalCount)
{
	assert(buffer.handle == 0);
	assert(normalCount > 0);

	const uint64_t bufferSize =
		static_cast<uint64_t>(normalCount) *
		sizeof(glm::vec4);

	if (bufferSize >
		static_cast<uint64_t>(
			std::numeric_limits<GLsizeiptr>::max()))
	{
		std::printf(
			"Smooth-normal buffer size exceeds GLsizeiptr.\n");

		return false;
	}

	glCreateBuffers(
		1,
		&buffer.handle);

	if (buffer.handle == 0)
	{
		std::printf(
			"Failed to create smooth-normal buffer.\n");

		return false;
	}

	glNamedBufferData(
		buffer.handle,
		static_cast<GLsizeiptr>(bufferSize),
		nullptr,
		GL_STATIC_DRAW);

	buffer.normalCount = normalCount;

	return true;
}

void destroyGpuSurfaceNormalBuffer(
	GpuSurfaceNormalBuffer& buffer)
{
	if (buffer.handle != 0)
	{
		glDeleteBuffers(
			1,
			&buffer.handle);
	}

	buffer = {};
}

/***********************************************************
* GPU Surface Normal Generation
************************************************************/

void generateGpuSphereSurfaceNormals(
	const GpuSurfaceNormalGenerator& generator,
	const GpuMesh& mesh,
	const GpuSurfaceNormalBuffer& normalBuffer,
	const ChunkCoord& chunkCoord,
	const SphereDensityField& sphere)
{
	assert(generator.program.handle != 0);
	assert(mesh.vertexBuffer != 0);
	assert(normalBuffer.handle != 0);

	assert(normalBuffer.normalCount == mesh.vertexCount);

	const uint32_t vertexCount =
		normalBuffer.normalCount;

	if (vertexCount == 0)
	{
		return;
	}

	glUseProgram(
		generator.program.handle);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		0,
		mesh.vertexBuffer);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		1,
		normalBuffer.handle);

	glUniform1ui(
		generator.vertexCountLocation,
		vertexCount);

	glUniform3i(
		generator.meshChunkLocation,
		chunkCoord.x,
		chunkCoord.y,
		chunkCoord.z);

	glUniform3i(
		generator.sphereCenterChunkLocation,
		sphere.center.chunk.x,
		sphere.center.chunk.y,
		sphere.center.chunk.z);

	glUniform3fv(
		generator.sphereCenterLocalLocation,
		1,
		glm::value_ptr(
			sphere.center.localPosition));

	if (generator.sphereRadiusLocation >= 0)
	{
		glUniform1f(
			generator.sphereRadiusLocation,
			sphere.radius);
	}

	const uint32_t groupCount =
		vertexCount / NORMAL_COMPUTE_WORKGROUP_SIZE +
		(vertexCount % NORMAL_COMPUTE_WORKGROUP_SIZE != 0
			? 1u
			: 0u);

	glDispatchCompute(
		groupCount,
		1,
		1);

	glMemoryBarrier(
		GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
		GL_BUFFER_UPDATE_BARRIER_BIT);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		0,
		0);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		1,
		0);
}
