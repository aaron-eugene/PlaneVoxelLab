///////////////////////////////////////////////////////////////////////////////
// tests/gpu_heightmap_normals_test.cpp
// ====================================
//
// Compares GPU heightmap gradients with CPU fractal-noise finite differences.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests/gpu_heightmap_normals_test.h"

#ifndef NDEBUG

#include "fields/field_generators.h"
#include "math/noise.h"
#include "renderer/gpu_noise_domain.h"
#include "renderer/shader.h"
#include "spatial/spatial_coordinates.h"

#include <glad/glad.h>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

/***********************************************************
* Test Types
************************************************************/

struct TestPosition
{
	int32_t chunkX = 0;
	float localX = 0.0f;
	int32_t chunkZ = 0;
	float localZ = 0.0f;
};

struct alignas(8) GpuTestInput
{
	uint32_t wholeXLow = 0;
	uint32_t wholeXHigh = 0;
	uint32_t wholeZLow = 0;
	uint32_t wholeZHigh = 0;
	float fractionalX = 0.0f;
	float fractionalZ = 0.0f;
};

struct alignas(16) GpuTestOutput
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float valid = 0.0f;
};

struct alignas(16) GpuOctaveData
{
	int32_t numerator = 1;
	int32_t denominator = 1;
	float amplitude = 1.0f;
	float reserved = 0.0f;
};

static_assert(sizeof(GpuTestInput) == 24);
static_assert(sizeof(GpuTestOutput) == 16);
static_assert(sizeof(GpuOctaveData) == 16);

/***********************************************************
* Test Data
************************************************************/

static constexpr std::array<TestPosition, 12> TEST_POSITIONS =
{ {
	{ 0, 0.0f, 0, 0.0f },
	{ 0, 0.0625f, 0, 0.125f },
	{ 0, 15.9375f, 0, 15.9375f },
	{ -1, 15.9375f, -1, 0.0625f },
	{ 1, 0.0625f, -1, 15.9375f },
	{ 25, 7.5f, -30, 3.25f },
	{ -25, 8.5f, 30, 11.25f },
	{ 1000, 4.5f, -1000, 11.75f },
	{ -100000, 4.5f, 100000, 11.75f },
	{ 268435456, 0.5f, -268435456, 0.5f },
	{ std::numeric_limits<int32_t>::max(), 15.5f,
		std::numeric_limits<int32_t>::min(), 0.5f },
	{ std::numeric_limits<int32_t>::min(), 0.5f,
		std::numeric_limits<int32_t>::max(), 15.5f }
} };

static constexpr float GRADIENT_STEP_METERS = 0.1f;
static constexpr float NORMAL_TOLERANCE = 0.003f;

/***********************************************************
* Shader Source Helpers
************************************************************/

static bool insertModule(
	std::string& computeSource,
	const char* path,
	const char* marker)
{
	std::string moduleSource = {};

	return
		loadShaderSourceFile(moduleSource, path) &&
		insertShaderSource(computeSource, marker, moduleSource);
}

static bool loadTestSource(
	std::string& computeSource)
{
	if (!loadShaderSourceFile(
		computeSource,
		"assets/shaders/tests/heightmap_normals_test.comp"))
	{
		return false;
	}

	return
		insertModule(computeSource,
			"assets/shaders/spatial/world_position.glsl",
			"// @WORLD_POSITION_IMPLEMENTATION@") &&
		insertModule(computeSource,
			"assets/shaders/spatial/world_metric_coordinate.glsl",
			"// @WORLD_METRIC_COORDINATE_IMPLEMENTATION@") &&
		insertModule(computeSource,
			"assets/shaders/math/wide_integer.glsl",
			"// @WIDE_INTEGER_IMPLEMENTATION@") &&
		insertModule(computeSource,
			"assets/shaders/math/noise_coordinates.glsl",
			"// @NOISE_COORDINATE_IMPLEMENTATION@") &&
		insertModule(computeSource,
			"assets/shaders/math/value_noise.glsl",
			"// @VALUE_NOISE_IMPLEMENTATION@") &&
		insertModule(computeSource,
			"assets/shaders/math/fractal_noise.glsl",
			"// @FRACTAL_NOISE_IMPLEMENTATION@") &&
		insertModule(computeSource,
			"assets/shaders/math/heightmap_normals.glsl",
			"// @HEIGHTMAP_NORMALS_IMPLEMENTATION@");
}

/***********************************************************
* CPU Reference
************************************************************/

static float sampleCpuFractalNoise(
	const WorldMetricCoordinate& x,
	const WorldMetricCoordinate& z,
	const HeightmapDensityField& heightmap)
{
	return sampleFractalValueNoise2d(
		x.wholeMeters,
		z.wholeMeters,
		x.fractionalMeter,
		z.fractionalMeter,
		heightmap.baseScale,
		heightmap.octaveCount,
		heightmap.persistence,
		heightmap.lacunarity,
		heightmap.seed);
}

static glm::vec3 sampleCpuHeightmapNormal(
	const TestPosition& position,
	const HeightmapDensityField& heightmap)
{
	const WorldMetricCoordinate x =
		getWorldMetricCoordinate(position.chunkX, position.localX);
	const WorldMetricCoordinate z =
		getWorldMetricCoordinate(position.chunkZ, position.localZ);

	const WorldMetricCoordinate left =
		offsetWorldMetricCoordinate(x, -GRADIENT_STEP_METERS);
	const WorldMetricCoordinate right =
		offsetWorldMetricCoordinate(x, GRADIENT_STEP_METERS);
	const WorldMetricCoordinate back =
		offsetWorldMetricCoordinate(z, -GRADIENT_STEP_METERS);
	const WorldMetricCoordinate front =
		offsetWorldMetricCoordinate(z, GRADIENT_STEP_METERS);

	const float noiseLeft = sampleCpuFractalNoise(left, z, heightmap);
	const float noiseRight = sampleCpuFractalNoise(right, z, heightmap);
	const float noiseBack = sampleCpuFractalNoise(x, back, heightmap);
	const float noiseFront = sampleCpuFractalNoise(x, front, heightmap);

	const float scale = heightmap.amplitude *
		(0.5f / GRADIENT_STEP_METERS);

	return glm::normalize(glm::vec3(
		-(noiseRight - noiseLeft) * scale,
		1.0f,
		-(noiseFront - noiseBack) * scale));
}

/***********************************************************
* GPU Heightmap Normal Validation
************************************************************/

bool validateGpuHeightmapNormals()
{
	HeightmapDensityField heightmap = {};
	heightmap.amplitude = 22.0f;
	heightmap.baseScale = { 1, 25 };
	heightmap.octaveCount = 5;
	heightmap.persistence = 0.5f;
	heightmap.lacunarity = { 2, 1 };
	heightmap.seed = 9109u;

	GpuHeightmapNoiseConfiguration prepared = {};
	const GpuNoiseDomainResult validation =
		prepareGpuHeightmapNoiseConfiguration(
			heightmap, getFullGpuNoiseDomain(), prepared);

	if (validation.failure != GpuNoiseDomainFailure::None)
	{
		return false;
	}

	std::array<GpuTestInput, TEST_POSITIONS.size()> inputs = {};
	std::array<GpuTestOutput, TEST_POSITIONS.size()> outputs = {};
	std::array<GpuOctaveData, MAX_GPU_NOISE_OCTAVES> octaves = {};

	for (std::size_t i = 0; i < inputs.size(); ++i)
	{
		const TestPosition& position = TEST_POSITIONS[i];
		const WorldMetricCoordinate x =
			getWorldMetricCoordinate(position.chunkX, position.localX);
		const WorldMetricCoordinate z =
			getWorldMetricCoordinate(position.chunkZ, position.localZ);
		const uint64_t bitsX = static_cast<uint64_t>(x.wholeMeters);
		const uint64_t bitsZ = static_cast<uint64_t>(z.wholeMeters);

		inputs[i].wholeXLow = static_cast<uint32_t>(bitsX);
		inputs[i].wholeXHigh = static_cast<uint32_t>(bitsX >> 32);
		inputs[i].wholeZLow = static_cast<uint32_t>(bitsZ);
		inputs[i].wholeZHigh = static_cast<uint32_t>(bitsZ >> 32);
		inputs[i].fractionalX = x.fractionalMeter;
		inputs[i].fractionalZ = z.fractionalMeter;
	}

	for (uint32_t i = 0; i < prepared.octaveCount; ++i)
	{
		octaves[i].numerator = prepared.octaves[i].numerator;
		octaves[i].denominator = prepared.octaves[i].denominator;
		octaves[i].amplitude = prepared.octaves[i].amplitude;
	}

	std::string source = {};
	if (!loadTestSource(source))
	{
		return false;
	}

	ShaderProgram program = {};
	GLuint buffers[3] = {};
	bool succeeded = false;

	do
	{
		if (!createComputeShaderProgram(program, source.c_str()))
		{
			break;
		}

		const int testCountLocation = getShaderUniformLocation(program, "uTestCount");
		const int octaveCountLocation = getShaderUniformLocation(program, "uNoiseOctaveCount");
		const int seedLocation = getShaderUniformLocation(program, "uNoiseSeed");
		const int totalLocation = getShaderUniformLocation(program, "uNoiseTotalAmplitude");
		const int amplitudeLocation = getShaderUniformLocation(program, "uHeightAmplitude");
		const int stepLocation = getShaderUniformLocation(program, "uGradientStepMeters");

		if (testCountLocation < 0 || octaveCountLocation < 0 ||
			seedLocation < 0 || totalLocation < 0 ||
			amplitudeLocation < 0 || stepLocation < 0)
		{
			break;
		}

		glCreateBuffers(3, buffers);
		if (buffers[0] == 0 || buffers[1] == 0 || buffers[2] == 0)
		{
			break;
		}

		glNamedBufferData(buffers[0], sizeof(inputs), inputs.data(), GL_STATIC_DRAW);
		glNamedBufferData(buffers[1], sizeof(outputs), nullptr, GL_DYNAMIC_READ);
		glNamedBufferData(buffers[2], sizeof(octaves), octaves.data(), GL_STATIC_DRAW);

		glUseProgram(program.handle);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers[0]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers[1]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffers[2]);

		glUniform1ui(testCountLocation, static_cast<GLuint>(inputs.size()));
		glUniform1ui(octaveCountLocation, prepared.octaveCount);
		glUniform1ui(seedLocation, prepared.seed);
		glUniform1f(totalLocation, prepared.totalAmplitude);
		glUniform1f(amplitudeLocation, prepared.heightAmplitude);
		glUniform1f(stepLocation, GRADIENT_STEP_METERS);

		const GLuint groupCount =
			static_cast<GLuint>((inputs.size() + 63u) / 64u);
		glDispatchCompute(groupCount, 1, 1);
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		glGetNamedBufferSubData(buffers[1], 0, sizeof(outputs), outputs.data());

		succeeded = true;
		for (std::size_t i = 0; i < outputs.size(); ++i)
		{
			const glm::vec3 expected =
				sampleCpuHeightmapNormal(TEST_POSITIONS[i], heightmap);
			const glm::vec3 actual =
				glm::vec3(outputs[i].x, outputs[i].y, outputs[i].z);

			if (outputs[i].valid != 1.0f ||
				!std::isfinite(actual.x) || !std::isfinite(actual.y) ||
				!std::isfinite(actual.z) ||
				glm::length(actual - expected) > NORMAL_TOLERANCE)
			{
				std::printf(
					"GPU heightmap normal test %zu failed. Error: %.9f, Valid: %.0f\n",
					i, glm::length(actual - expected), outputs[i].valid);
				succeeded = false;
				break;
			}
		}
	} while (false);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
	glUseProgram(0);

	for (GLuint& buffer : buffers)
	{
		if (buffer != 0)
		{
			glDeleteBuffers(1, &buffer);
		}
	}

	destroyShaderProgram(program);

	if (succeeded)
	{
		std::printf(
			"GPU heightmap-normal validation passed: %zu cases.\n",
			TEST_POSITIONS.size());
	}

	return succeeded;
}

#endif
