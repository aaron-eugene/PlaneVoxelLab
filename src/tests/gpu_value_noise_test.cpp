///////////////////////////////////////////////////////////////////////////////
// tests/gpu_value_noise_test.cpp
// =============================
//
// Compares GPU value-noise evaluation against the CPU reference.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests/gpu_value_noise_test.h"

#ifndef NDEBUG

#include "math/noise.h"
#include "renderer/shader.h"

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

struct ValueNoiseTestCase
{
	int64_t cellX = 0;
	int64_t cellZ = 0;
	float localX = 0.0f;
	float localZ = 0.0f;
	uint32_t seed = 0;
};

// Matches the shader's std430 TestInput layout (32 bytes).
struct alignas(8) ValueNoiseTestInput
{
	uint32_t cellXLow = 0;
	uint32_t cellXHigh = 0;
	uint32_t cellZLow = 0;
	uint32_t cellZHigh = 0;
	float localX = 0.0f;
	float localZ = 0.0f;
	uint32_t seed = 0;
	uint32_t reserved = 0;
};

static_assert(sizeof(ValueNoiseTestInput) == 32);
static_assert(offsetof(ValueNoiseTestInput, cellZLow) == 8);
static_assert(offsetof(ValueNoiseTestInput, localX) == 16);
static_assert(offsetof(ValueNoiseTestInput, seed) == 24);

/***********************************************************
* Test Data
************************************************************/

// Keep cell coordinates below INT64_MAX: CPU sampleValueNoise2d()
// evaluates cell + 1 for each axis.
static constexpr std::array<ValueNoiseTestCase, 28> TEST_CASES =
{ {
	{ 0, 0, 0.0f, 0.0f, 9109u },
	{ 0, 0, 0.5f, 0.5f, 9109u },
	{ 0, 0, 0.25f, 0.75f, 9109u },
	{ 0, 0, 0.99999994f, 0.00000011920929f, 9109u },
	{ 1, 0, 0.0f, 0.0f, 9109u },
	{ 0, 1, 0.0f, 0.0f, 9109u },
	{ -1, 0, 0.5f, 0.5f, 9109u },
	{ 0, -1, 0.5f, 0.5f, 9109u },
	{ -1, -1, 0.0f, 0.0f, 9109u },
	{ -2, -3, 0.125f, 0.875f, 9109u },
	{ 19, 25, 0.125f, 0.375f, 9109u },
	{ -19, 25, 0.625f, 0.75f, 9109u },
	{ 19, -25, 0.875f, 0.125f, 9109u },
	{ -19, -25, 0.375f, 0.625f, 9109u },
	{ 4294967295LL, 7, 0.5f, 0.25f, 9109u },
	{ 4294967296LL, 7, 0.5f, 0.25f, 9109u },
	{ -4294967296LL, -7, 0.5f, 0.25f, 9109u },
	{ -4294967297LL, -7, 0.5f, 0.25f, 9109u },
	{ 2147483647LL, -2147483648LL, 0.25f, 0.75f, 9109u },
	{ -2147483648LL, 2147483647LL, 0.75f, 0.25f, 9109u },
	{ 34359738367LL, -34359738368LL, 0.5f, 0.5f, 9109u },
	{ -34359738368LL, 34359738367LL, 0.5f, 0.5f, 9109u },
	{ std::numeric_limits<int64_t>::max() - 1, 0, 0.5f, 0.5f, 9109u },
	{ std::numeric_limits<int64_t>::min(), 0, 0.5f, 0.5f, 9109u },
	{ 0, 0, 0.5f, 0.5f, 0u },
	{ 0, 0, 0.5f, 0.5f, 1u },
	{ 0, 0, 0.5f, 0.5f, std::numeric_limits<uint32_t>::max() },
	{ -123456789012LL, 987654321011LL, 0.33333334f, 0.66666669f, 123456789u }
} };

static constexpr float VALUE_NOISE_TOLERANCE = 0.000001f;

/***********************************************************
* Test Shader Helpers
************************************************************/

static bool insertValueNoiseTestSource(
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

static bool loadGpuValueNoiseTestSource(
	std::string& computeSource)
{
	if (!loadShaderSourceFile(
		computeSource,
		"assets/shaders/tests/value_noise_test.comp"))
	{
		return false;
	}

	return
		insertValueNoiseTestSource(
			computeSource,
			"assets/shaders/spatial/world_position.glsl",
			"// @WORLD_POSITION_IMPLEMENTATION@") &&
		insertValueNoiseTestSource(
			computeSource,
			"assets/shaders/spatial/world_metric_coordinate.glsl",
			"// @WORLD_METRIC_COORDINATE_IMPLEMENTATION@") &&
		insertValueNoiseTestSource(
			computeSource,
			"assets/shaders/math/wide_integer.glsl",
			"// @WIDE_INTEGER_IMPLEMENTATION@") &&
		insertValueNoiseTestSource(
			computeSource,
			"assets/shaders/math/noise_coordinates.glsl",
			"// @NOISE_COORDINATE_IMPLEMENTATION@") &&
		insertValueNoiseTestSource(
			computeSource,
			"assets/shaders/math/value_noise.glsl",
			"// @VALUE_NOISE_IMPLEMENTATION@");
}

/***********************************************************
* CPU Reference Comparison
************************************************************/

static bool validateValueNoiseResults(
	const std::array<uint32_t, TEST_CASES.size()>& outputs)
{
	for (std::size_t index = 0;
		index < TEST_CASES.size();
		++index)
	{
		const ValueNoiseTestCase& test = TEST_CASES[index];

		NoisePosition2d position = {};
		position.cellX = test.cellX;
		position.cellZ = test.cellZ;
		position.localX = test.localX;
		position.localZ = test.localZ;

		const float expected = sampleValueNoise2d(position, test.seed);

		float actual = 0.0f;
		std::memcpy(&actual, &outputs[index], sizeof(actual));

		if (!std::isfinite(actual) ||
			std::fabs(expected - actual) > VALUE_NOISE_TOLERANCE)
		{
			std::printf(
				"GPU value-noise test %zu failed.\n"
				"  Cell X: %lld  Cell Z: %lld\n"
				"  Local: (%.9f, %.9f)  Seed: %u\n"
				"  Expected: %.9f  Actual: %.9f  Error: %.9f\n",
				index,
				static_cast<long long>(test.cellX),
				static_cast<long long>(test.cellZ),
				test.localX,
				test.localZ,
				test.seed,
				expected,
				actual,
				std::fabs(expected - actual));

			return false;
		}
	}

	return true;
}

/***********************************************************
* GPU Value Noise Validation
************************************************************/

bool validateGpuValueNoise()
{
	std::string computeSource = {};

	if (!loadGpuValueNoiseTestSource(computeSource))
	{
		return false;
	}

	std::array<ValueNoiseTestInput, TEST_CASES.size()> inputs = {};
	std::array<uint32_t, TEST_CASES.size()> outputs = {};

	for (std::size_t index = 0;
		index < TEST_CASES.size();
		++index)
	{
		const ValueNoiseTestCase& test = TEST_CASES[index];
		ValueNoiseTestInput& input = inputs[index];

		const uint64_t xBits = static_cast<uint64_t>(test.cellX);
		const uint64_t zBits = static_cast<uint64_t>(test.cellZ);

		input.cellXLow = static_cast<uint32_t>(xBits);
		input.cellXHigh = static_cast<uint32_t>(xBits >> 32);
		input.cellZLow = static_cast<uint32_t>(zBits);
		input.cellZHigh = static_cast<uint32_t>(zBits >> 32);
		input.localX = test.localX;
		input.localZ = test.localZ;
		input.seed = test.seed;
	}

	ShaderProgram program = {};
	GLuint buffers[2] = {};
	bool succeeded = false;

	do
	{
		if (!createComputeShaderProgram(
			program,
			computeSource.c_str()))
		{
			break;
		}

		const int testCountLocation =
			getShaderUniformLocation(
				program,
				"uTestCount");

		if (testCountLocation < 0)
		{
			break;
		}

		glCreateBuffers(2, buffers);

		if (buffers[0] == 0 || buffers[1] == 0)
		{
			break;
		}

		glNamedBufferData(
			buffers[0],
			sizeof(inputs),
			inputs.data(),
			GL_STATIC_DRAW);

		glNamedBufferData(
			buffers[1],
			sizeof(outputs),
			nullptr,
			GL_DYNAMIC_READ);

		glUseProgram(program.handle);

		glBindBufferBase(
			GL_SHADER_STORAGE_BUFFER,
			0,
			buffers[0]);

		glBindBufferBase(
			GL_SHADER_STORAGE_BUFFER,
			1,
			buffers[1]);

		glUniform1ui(
			testCountLocation,
			static_cast<GLuint>(TEST_CASES.size()));

		const GLuint groupCount =
			static_cast<GLuint>(
				(TEST_CASES.size() + 63u) / 64u);

		glDispatchCompute(groupCount, 1, 1);

		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

		glGetNamedBufferSubData(
			buffers[1],
			0,
			sizeof(outputs),
			outputs.data());

		succeeded = validateValueNoiseResults(outputs);
	} while (false);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
	glUseProgram(0);

	if (buffers[0] != 0)
	{
		glDeleteBuffers(1, &buffers[0]);
	}

	if (buffers[1] != 0)
	{
		glDeleteBuffers(1, &buffers[1]);
	}

	destroyShaderProgram(program);

	if (succeeded)
	{
		std::printf(
			"GPU value-noise validation passed: %zu cases.\n",
			TEST_CASES.size());
	}

	return succeeded;
}

#endif
