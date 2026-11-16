
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
#include "renderer/gpu_noise_domain.h"
#include "renderer/shader.h"

#include <glad/glad.h>

#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
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

static constexpr const char* HEIGHTMAP_NORMAL_COMPUTE_SHADER_PATH =
"assets/shaders/heightmap_surface_normals.comp";

static constexpr float HEIGHTMAP_GRADIENT_STEP_METERS = 0.1f;

/***********************************************************
* Compute Source Helpers
************************************************************/

static bool insertGpuNormalShaderModule(
	std::string& computeSource,
	const char* shaderPath,
	const char* marker)
{
	std::string moduleSource = {};

	if (!loadShaderSourceFile(
		moduleSource,
		shaderPath))
	{
		return false;
	}

	return insertShaderSource(
		computeSource,
		marker,
		moduleSource);
}

static bool loadGpuHeightmapNormalComputeSource(
	std::string& computeSource)
{
	if (!loadShaderSourceFile(
		computeSource,
		HEIGHTMAP_NORMAL_COMPUTE_SHADER_PATH))
	{
		return false;
	}

	return
		insertGpuNormalShaderModule(
			computeSource,
			"assets/shaders/spatial/world_position.glsl",
			"// @WORLD_POSITION_IMPLEMENTATION@") &&
		insertGpuNormalShaderModule(
			computeSource,
			"assets/shaders/spatial/world_metric_coordinate.glsl",
			"// @WORLD_METRIC_COORDINATE_IMPLEMENTATION@") &&
		insertGpuNormalShaderModule(
			computeSource,
			"assets/shaders/math/wide_integer.glsl",
			"// @WIDE_INTEGER_IMPLEMENTATION@") &&
		insertGpuNormalShaderModule(
			computeSource,
			"assets/shaders/math/noise_coordinates.glsl",
			"// @NOISE_COORDINATE_IMPLEMENTATION@") &&
		insertGpuNormalShaderModule(
			computeSource,
			"assets/shaders/math/value_noise.glsl",
			"// @VALUE_NOISE_IMPLEMENTATION@") &&
		insertGpuNormalShaderModule(
			computeSource,
			"assets/shaders/math/fractal_noise.glsl",
			"// @FRACTAL_NOISE_IMPLEMENTATION@") &&
		insertGpuNormalShaderModule(
			computeSource,
			"assets/shaders/math/heightmap_normals.glsl",
			"// @HEIGHTMAP_NORMALS_IMPLEMENTATION@");
}

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
* Specific Density Field Initialization Helpers
************************************************************/

static bool initializeGpuSphereNormalProgram(
	GpuSphereNormalProgram& sphere)
{
	assert(sphere.program.handle == 0);

	std::string computeSource = {};

	if (!loadGpuSurfaceNormalComputeSource(
		computeSource,
		SPHERE_DENSITY_SHADER_PATH))
	{
		return false;
	}

	if (!createComputeShaderProgram(
		sphere.program,
		computeSource.c_str()))
	{
		return false;
	}

	sphere.vertexCountLocation =
		getShaderUniformLocation(
			sphere.program,
			"uVertexCount");

	sphere.meshChunkLocation =
		getShaderUniformLocation(
			sphere.program,
			"uMeshChunk");

	sphere.centerChunkLocation =
		getShaderUniformLocation(
			sphere.program,
			"uSphereCenterChunk");

	sphere.centerLocalLocation =
		getShaderUniformLocation(
			sphere.program,
			"uSphereCenterLocal");

	sphere.radiusLocation =
		getShaderUniformLocation(
			sphere.program,
			"uSphereRadius");

	if (sphere.vertexCountLocation < 0 ||
		sphere.meshChunkLocation < 0 ||
		sphere.centerChunkLocation < 0 ||
		sphere.centerLocalLocation < 0)
	{
		destroyShaderProgram(sphere.program);
		sphere = {};

		return false;
	}

	return true;
}


static bool initializeGpuHeightmapNormalProgram(
	GpuHeightmapNormalProgram& heightmap)
{
	assert(heightmap.program.handle == 0);
	assert(heightmap.octaveBuffer == 0);

	std::string computeSource = {};

	if (!loadGpuHeightmapNormalComputeSource(
		computeSource))
	{
		return false;
	}

	if (!createComputeShaderProgram(
		heightmap.program,
		computeSource.c_str()))
	{
		return false;
	}

	heightmap.vertexCountLocation =
		getShaderUniformLocation(
			heightmap.program, "uVertexCount");

	heightmap.meshChunkLocation =
		getShaderUniformLocation(
			heightmap.program, "uMeshChunk");

	heightmap.octaveCountLocation =
		getShaderUniformLocation(
			heightmap.program, "uNoiseOctaveCount");

	heightmap.seedLocation =
		getShaderUniformLocation(
			heightmap.program, "uNoiseSeed");

	heightmap.totalAmplitudeLocation =
		getShaderUniformLocation(
			heightmap.program, "uNoiseTotalAmplitude");

	heightmap.heightAmplitudeLocation =
		getShaderUniformLocation(
			heightmap.program, "uHeightAmplitude");

	heightmap.gradientStepLocation =
		getShaderUniformLocation(
			heightmap.program, "uGradientStepMeters");

	if (heightmap.vertexCountLocation < 0 ||
		heightmap.meshChunkLocation < 0 ||
		heightmap.octaveCountLocation < 0 ||
		heightmap.seedLocation < 0 ||
		heightmap.totalAmplitudeLocation < 0 ||
		heightmap.heightAmplitudeLocation < 0 ||
		heightmap.gradientStepLocation < 0)
	{
		return false;
	}

	glCreateBuffers(1, &heightmap.octaveBuffer);

	if (heightmap.octaveBuffer == 0)
	{
		return false;
	}

	glNamedBufferData(
		heightmap.octaveBuffer,
		static_cast<GLsizeiptr>(
			MAX_GPU_NOISE_OCTAVES * 16u),
		nullptr,
		GL_DYNAMIC_DRAW);

	return true;
}

/***********************************************************
* GPU Surface Normal Generator Lifecycle
************************************************************/

bool initializeGpuSurfaceNormalGenerator(
	GpuSurfaceNormalGenerator& generator)
{
	if (!initializeGpuSphereNormalProgram(
		generator.sphere))
	{
		shutdownGpuSurfaceNormalGenerator(generator);
		return false;
	}

	if (!initializeGpuHeightmapNormalProgram(
		generator.heightmap))
	{
		shutdownGpuSurfaceNormalGenerator(generator);
		return false;
	}

	return true;
}

void shutdownGpuSurfaceNormalGenerator(
	GpuSurfaceNormalGenerator& generator)
{
	destroyShaderProgram(generator.sphere.program);
	destroyShaderProgram(generator.heightmap.program);

	if (generator.heightmap.octaveBuffer != 0)
	{
		glDeleteBuffers(
			1,
			&generator.heightmap.octaveBuffer);
	}

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
* GPU Sphere Surface Normal Generation
************************************************************/

void generateGpuSphereSurfaceNormals(
	const GpuSurfaceNormalGenerator& generator,
	const GpuMesh& mesh,
	const GpuSurfaceNormalBuffer& normalBuffer,
	const ChunkCoord& chunkCoord,
	const SphereDensityField& sphere)
{
	const GpuSphereNormalProgram& sphereProgram =
		generator.sphere;

	assert(sphereProgram.program.handle != 0);
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
		sphereProgram.program.handle);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		0,
		mesh.vertexBuffer);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		1,
		normalBuffer.handle);

	glUniform1ui(
		sphereProgram.vertexCountLocation,
		vertexCount);

	glUniform3i(
		sphereProgram.meshChunkLocation,
		chunkCoord.x,
		chunkCoord.y,
		chunkCoord.z);

	glUniform3i(
		sphereProgram.centerChunkLocation,
		sphere.center.chunk.x,
		sphere.center.chunk.y,
		sphere.center.chunk.z);

	glUniform3fv(
		sphereProgram.centerLocalLocation,
		1,
		glm::value_ptr(
			sphere.center.localPosition));

	if (sphereProgram.radiusLocation >= 0)
	{
		glUniform1f(
			sphereProgram.radiusLocation,
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

/***********************************************************
* GPU Heightmap Configuration
************************************************************/

struct alignas(16) GpuHeightmapOctaveData
{
	int32_t numerator = 1;
	int32_t denominator = 1;
	float amplitude = 1.0f;
	float reserved = 0.0f;
};

static_assert(sizeof(GpuHeightmapOctaveData) == 16);

bool prepareGpuHeightmapSurfaceNormals(
	GpuSurfaceNormalGenerator& generator,
	const GpuHeightmapNoiseConfiguration& configuration)
{
	GpuHeightmapNormalProgram& heightmap =
		generator.heightmap;

	assert(heightmap.program.handle != 0);
	assert(heightmap.octaveBuffer != 0);

	if (configuration.octaveCount == 0 ||
		configuration.octaveCount > MAX_GPU_NOISE_OCTAVES ||
		!(configuration.totalAmplitude > 0.0f))
	{
		assert(false &&
			"Invalid prepared GPU heightmap configuration");

		return false;
	}

	heightmap.configured = false;

	std::array<GpuHeightmapOctaveData, MAX_GPU_NOISE_OCTAVES>
		octaveData = {};

	for (uint32_t octave = 0;
		octave < configuration.octaveCount;
		++octave)
	{
		octaveData[octave].numerator =
			configuration.octaves[octave].numerator;

		octaveData[octave].denominator =
			configuration.octaves[octave].denominator;

		octaveData[octave].amplitude =
			configuration.octaves[octave].amplitude;
	}

	glNamedBufferSubData(
		heightmap.octaveBuffer,
		0,
		static_cast<GLsizeiptr>(
			configuration.octaveCount *
			sizeof(GpuHeightmapOctaveData)),
		octaveData.data());

	const GLuint program =
		heightmap.program.handle;

	glProgramUniform1ui(
		program,
		heightmap.octaveCountLocation,
		configuration.octaveCount);

	glProgramUniform1ui(
		program,
		heightmap.seedLocation,
		configuration.seed);

	glProgramUniform1f(
		program,
		heightmap.totalAmplitudeLocation,
		configuration.totalAmplitude);

	glProgramUniform1f(
		program,
		heightmap.heightAmplitudeLocation,
		configuration.heightAmplitude);

	glProgramUniform1f(
		program,
		heightmap.gradientStepLocation,
		HEIGHTMAP_GRADIENT_STEP_METERS);

	heightmap.configured = true;

	return true;
}

void generateGpuHeightmapSurfaceNormals(
	const GpuSurfaceNormalGenerator& generator,
	const GpuMesh& mesh,
	const GpuSurfaceNormalBuffer& normalBuffer,
	const ChunkCoord& chunkCoord)
{
	const GpuHeightmapNormalProgram& heightmap =
		generator.heightmap;

	assert(heightmap.program.handle != 0);
	assert(heightmap.configured);
	assert(heightmap.octaveBuffer != 0);

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
		heightmap.program.handle);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		0,
		mesh.vertexBuffer);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		1,
		normalBuffer.handle);

	glBindBufferBase(
		GL_SHADER_STORAGE_BUFFER,
		2,
		heightmap.octaveBuffer);

	glUniform1ui(
		heightmap.vertexCountLocation,
		vertexCount);

	glUniform3i(
		heightmap.meshChunkLocation,
		chunkCoord.x,
		chunkCoord.y,
		chunkCoord.z);

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

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
}
