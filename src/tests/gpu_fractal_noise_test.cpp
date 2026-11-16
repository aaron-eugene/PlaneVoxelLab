///////////////////////////////////////////////////////////////////////////////
// tests/gpu_fractal_noise_test.cpp
// ===============================
//
// Compares GPU fractal value noise against the existing CPU reference.
// The GPU receives the actual validated octave parameters.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests/gpu_fractal_noise_test.h"

#ifndef NDEBUG

#include "fields/field_generators.h"
#include "math/noise.h"
#include "renderer/gpu_noise_domain.h"
#include "renderer/shader.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"

#include <glad/glad.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

/***********************************************************
* Test Types
************************************************************/

struct FractalNoiseTestPosition
{
	int32_t chunkX = 0;
	float localX = 0.0f;
	int32_t chunkZ = 0;
	float localZ = 0.0f;
};

// std430: uvec2, uvec2, float, float; array stride is 24 bytes.
struct alignas(8) FractalNoiseTestInput
{
	uint32_t wholeXLow = 0;
	uint32_t wholeXHigh = 0;
	uint32_t wholeZLow = 0;
	uint32_t wholeZHigh = 0;
	float fractionalX = 0.0f;
	float fractionalZ = 0.0f;
};

// std430: uvec2 result; value bits followed by validity.
struct alignas(8) FractalNoiseTestOutput
{
	uint32_t valueBits = 0;
	uint32_t valid = 0;
};

// std430: each octave has a 16-byte stride.
struct alignas(16) FractalNoiseGpuOctave
{
	int32_t numerator = 1;
	int32_t denominator = 1;
	float amplitude = 1.0f;
	float reserved = 0.0f;
};

struct FractalNoiseTestConfiguration
{
	NoiseScale baseScale = { 1, 25 };
	NoiseScale lacunarity = { 2, 1 };
	uint32_t octaveCount = 5;
	float persistence = 0.5f;
	uint32_t seed = 9109u;
};

static_assert(sizeof(FractalNoiseTestInput) == 24);
static_assert(offsetof(FractalNoiseTestInput, fractionalX) == 16);
static_assert(sizeof(FractalNoiseTestOutput) == 8);
static_assert(sizeof(FractalNoiseGpuOctave) == 16);

/***********************************************************
* Test Data
************************************************************/

static constexpr std::array<FractalNoiseTestPosition, 14> TEST_POSITIONS =
{ {
	{ 0, 0.0f, 0, 0.0f },
	{ 0, 0.5f, 0, 0.5f },
	{ 0, 15.5f, 0, 1.25f },
	{ 1, 4.0f, -1, 15.5f },
	{ -1, 15.5f, -1, 15.5f },
	{ -2, 11.5f, 2, 12.0f },
	{ 25, 0.125f, -25, 15.875f },
	{ -25, 15.875f, 25, 0.125f },
	{ 1000, 4.5f, -1000, 11.75f },
	{ -100000, 4.5f, 100000, 11.75f },
	{ 268435456, 0.5f, -268435456, 0.5f },
	{ -268435457, 15.5f, 268435455, 15.5f },
	{ std::numeric_limits<int32_t>::max(), 15.5f,
		std::numeric_limits<int32_t>::min(), 0.0f },
	{ std::numeric_limits<int32_t>::min(), 15.5f,
		std::numeric_limits<int32_t>::max(), 0.0f }
} };

static constexpr std::array<FractalNoiseTestConfiguration, 5> TEST_CONFIGURATIONS =
{ {
	{ { 1, 25 }, { 2, 1 }, 5, 0.5f, 9109u },
	{ { 1, 20 }, { 2, 1 }, 1, 0.5f, 9109u },
	{ { 3, 40 }, { 3, 2 }, 6, 0.7f, 123456789u },
	{ { 1, 64 }, { 4, 1 }, 8, 0.25f, 0u },
	{ { 1, 25 }, { 2, 1 }, 5, 0.0f,
		std::numeric_limits<uint32_t>::max() }
} };

static constexpr float FRACTAL_NOISE_TOLERANCE = 0.00002f;

/***********************************************************
* Shader Source Helpers
************************************************************/

static bool insertFractalNoiseSource(
	std::string& computeSource,
	const char* path,
	const char* marker)
{
	std::string moduleSource = {};

	if (!loadShaderSourceFile(moduleSource, path))
	{
		return false;
	}

	return insertShaderSource(computeSource, marker, moduleSource);
}

static bool loadGpuFractalNoiseTestSource(
	std::string& computeSource)
{
	if (!loadShaderSourceFile(
		computeSource,
		"assets/shaders/tests/fractal_noise_test.comp"))
	{
		return false;
	}

	return
		insertFractalNoiseSource(
			computeSource,
			"assets/shaders/spatial/world_position.glsl",
			"// @WORLD_POSITION_IMPLEMENTATION@") &&
		insertFractalNoiseSource(
			computeSource,
			"assets/shaders/spatial/world_metric_coordinate.glsl",
			"// @WORLD_METRIC_COORDINATE_IMPLEMENTATION@") &&
		insertFractalNoiseSource(
			computeSource,
			"assets/shaders/math/wide_integer.glsl",
			"// @WIDE_INTEGER_IMPLEMENTATION@") &&
		insertFractalNoiseSource(
			computeSource,
			"assets/shaders/math/noise_coordinates.glsl",
			"// @NOISE_COORDINATE_IMPLEMENTATION@") &&
		insertFractalNoiseSource(
			computeSource,
			"assets/shaders/math/value_noise.glsl",
			"// @VALUE_NOISE_IMPLEMENTATION@") &&
		insertFractalNoiseSource(
			computeSource,
			"assets/shaders/math/fractal_noise.glsl",
			"// @FRACTAL_NOISE_IMPLEMENTATION@");
}

/***********************************************************
* Test Input Preparation
************************************************************/

static void writeWorldMetricCoordinate(
	const WorldMetricCoordinate& coordinate,
	uint32_t& low,
	uint32_t& high,
	float& fractional)
{
	const uint64_t bits = static_cast<uint64_t>(coordinate.wholeMeters);
	low = static_cast<uint32_t>(bits);
	high = static_cast<uint32_t>(bits >> 32);
	fractional = coordinate.fractionalMeter;
}

static void prepareTestInputs(
	std::array<FractalNoiseTestInput, TEST_POSITIONS.size()>& inputs)
{
	for (std::size_t index = 0; index < TEST_POSITIONS.size(); ++index)
	{
		const FractalNoiseTestPosition& position = TEST_POSITIONS[index];
		FractalNoiseTestInput& input = inputs[index];

		const WorldMetricCoordinate x =
			getWorldMetricCoordinate(position.chunkX, position.localX);
		const WorldMetricCoordinate z =
			getWorldMetricCoordinate(position.chunkZ, position.localZ);

		writeWorldMetricCoordinate(
			x, input.wholeXLow, input.wholeXHigh, input.fractionalX);
		writeWorldMetricCoordinate(
			z, input.wholeZLow, input.wholeZHigh, input.fractionalZ);
	}
}

static bool prepareTestConfiguration(
	const FractalNoiseTestConfiguration& test,
	GpuHeightmapNoiseConfiguration& configuration)
{
	HeightmapDensityField heightmap = {};
	heightmap.baseScale = test.baseScale;
	heightmap.lacunarity = test.lacunarity;
	heightmap.octaveCount = test.octaveCount;
	heightmap.persistence = test.persistence;
	heightmap.seed = test.seed;

	const GpuNoiseDomainResult result =
		prepareGpuHeightmapNoiseConfiguration(
			heightmap,
			getFullGpuNoiseDomain(),
			configuration);

	if (result.failure != GpuNoiseDomainFailure::None)
	{
		std::printf(
			"GPU fractal-noise test configuration rejected at octave %u: %s\n",
			result.failedOctave,
			getGpuNoiseDomainFailureDescription(result.failure));
		return false;
	}

	return true;
}

/***********************************************************
* CPU Reference Comparison
************************************************************/

static bool validateFractalNoiseResults(
	const FractalNoiseTestConfiguration& test,
	std::size_t configurationIndex,
	const std::array<FractalNoiseTestOutput, TEST_POSITIONS.size()>& outputs)
{
	for (std::size_t index = 0; index < TEST_POSITIONS.size(); ++index)
	{
		const FractalNoiseTestPosition& position = TEST_POSITIONS[index];
		const WorldMetricCoordinate x =
			getWorldMetricCoordinate(position.chunkX, position.localX);
		const WorldMetricCoordinate z =
			getWorldMetricCoordinate(position.chunkZ, position.localZ);

		const float expected = sampleFractalValueNoise2d(
			x.wholeMeters,
			z.wholeMeters,
			x.fractionalMeter,
			z.fractionalMeter,
			test.baseScale,
			test.octaveCount,
			test.persistence,
			test.lacunarity,
			test.seed);

		float actual = 0.0f;
		std::memcpy(&actual, &outputs[index].valueBits, sizeof(actual));

		if (outputs[index].valid != 1u ||
			!std::isfinite(actual) ||
			!std::isfinite(expected) ||
			std::fabs(expected - actual) > FRACTAL_NOISE_TOLERANCE)
		{
			std::printf(
				"GPU fractal-noise configuration %zu, position %zu failed.\n"
				"  Chunks X/Z: %d / %d  Locals: %.6f / %.6f\n"
				"  Scale: %lld/%lld  Lacunarity: %lld/%lld\n"
				"  Octaves: %u  Persistence: %.6f  Seed: %u\n"
				"  Expected: %.9f  Actual: %.9f  Valid: %u\n",
				configurationIndex,
				index,
				position.chunkX,
				position.chunkZ,
				position.localX,
				position.localZ,
				static_cast<long long>(test.baseScale.numerator),
				static_cast<long long>(test.baseScale.denominator),
				static_cast<long long>(test.lacunarity.numerator),
				static_cast<long long>(test.lacunarity.denominator),
				test.octaveCount,
				test.persistence,
				test.seed,
				expected,
				actual,
				outputs[index].valid);
			return false;
		}
	}

	return true;
}

/***********************************************************
* GPU Fractal Noise Validation
************************************************************/

bool validateGpuFractalNoise()
{
	std::string computeSource = {};

	if (!loadGpuFractalNoiseTestSource(computeSource))
	{
		return false;
	}

	std::array<FractalNoiseTestInput, TEST_POSITIONS.size()> inputs = {};
	std::array<FractalNoiseTestOutput, TEST_POSITIONS.size()> outputs = {};
	prepareTestInputs(inputs);

	ShaderProgram program = {};
	GLuint buffers[3] = {};
	bool succeeded = false;

	do
	{
		if (!createComputeShaderProgram(program, computeSource.c_str()))
		{
			break;
		}

		const int testCountLocation = getShaderUniformLocation(program, "uTestCount");
		const int octaveCountLocation = getShaderUniformLocation(program, "uNoiseOctaveCount");
		const int seedLocation = getShaderUniformLocation(program, "uNoiseSeed");
		const int totalAmplitudeLocation = getShaderUniformLocation(program, "uNoiseTotalAmplitude");

		if (testCountLocation < 0 || octaveCountLocation < 0 ||
			seedLocation < 0 || totalAmplitudeLocation < 0)
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
		glNamedBufferData(
			buffers[2],
			static_cast<GLsizeiptr>(sizeof(FractalNoiseGpuOctave) * MAX_GPU_NOISE_OCTAVES),
			nullptr,
			GL_DYNAMIC_DRAW);

		glUseProgram(program.handle);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers[0]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers[1]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffers[2]);
		glUniform1ui(testCountLocation, static_cast<GLuint>(TEST_POSITIONS.size()));

		succeeded = true;

		for (std::size_t configIndex = 0;
			configIndex < TEST_CONFIGURATIONS.size();
			++configIndex)
		{
			const FractalNoiseTestConfiguration& test =
				TEST_CONFIGURATIONS[configIndex];

			GpuHeightmapNoiseConfiguration prepared = {};
			if (!prepareTestConfiguration(test, prepared))
			{
				succeeded = false;
				break;
			}

			std::array<FractalNoiseGpuOctave, MAX_GPU_NOISE_OCTAVES> gpuOctaves = {};

			for (uint32_t octave = 0; octave < prepared.octaveCount; ++octave)
			{
				gpuOctaves[octave].numerator = prepared.octaves[octave].numerator;
				gpuOctaves[octave].denominator = prepared.octaves[octave].denominator;
				gpuOctaves[octave].amplitude = prepared.octaves[octave].amplitude;
			}

			glNamedBufferSubData(
				buffers[2],
				0,
				static_cast<GLsizeiptr>(prepared.octaveCount * sizeof(FractalNoiseGpuOctave)),
				gpuOctaves.data());

			glUniform1ui(octaveCountLocation, prepared.octaveCount);
			glUniform1ui(seedLocation, prepared.seed);
			glUniform1f(totalAmplitudeLocation, prepared.totalAmplitude);

			const GLuint groupCount =
				static_cast<GLuint>((TEST_POSITIONS.size() + 63u) / 64u);

			glDispatchCompute(groupCount, 1, 1);
			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			glGetNamedBufferSubData(buffers[1], 0, sizeof(outputs), outputs.data());

			if (!validateFractalNoiseResults(test, configIndex, outputs))
			{
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
			"GPU fractal-noise validation passed: %zu cases (%zu configurations x %zu positions).\n",
			TEST_CONFIGURATIONS.size() * TEST_POSITIONS.size(),
			TEST_CONFIGURATIONS.size(),
			TEST_POSITIONS.size());
	}

	return succeeded;
}

#endif
