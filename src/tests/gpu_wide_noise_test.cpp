///////////////////////////////////////////////////////////////////////////////
// tests/gpu_wide_noise_test.cpp
// =============================
// Validates GPU wide noise-cell conversion against the CPU noise system.
///////////////////////////////////////////////////////////////////////////////

#include "tests/gpu_wide_noise_test.h"

#ifndef NDEBUG

#include "math/noise.h"
#include "renderer/shader.h"
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

struct WideNoiseTestInput
{
	int32_t chunkCoordinate = 0;
	float localPosition = 0.0f;
	int32_t numerator = 1;
	int32_t denominator = 1;
};

struct alignas(16) WideNoiseTestOutput
{
	uint32_t cellLow = 0;
	uint32_t cellHigh = 0;
	uint32_t localBits = 0;
	uint32_t valid = 0;
};

static_assert(sizeof(WideNoiseTestInput) == 16);
static_assert(sizeof(WideNoiseTestOutput) == 16);

/***********************************************************
* Test Data
************************************************************/

static constexpr std::array<WideNoiseTestInput, 32> TEST_INPUTS =
{ {
	{ 0, 0.0f, 1, 20 },
	{ 0, 5.0f, 1, 20 },
	{ 1, 4.0f, 1, 20 },
	{ -1, 15.5f, 1, 20 },
	{ -2, 11.5f, 1, 20 },
	{ -2, 12.0f, 1, 20 },
	{ 0, 15.5f, 2, 25 },
	{ -1, 0.0f, 2, 25 },
	{ -1, 15.875f, 16, 25 },
	{ 1, 0.125f, 16, 25 },
	{ 1000, 4.5f, 16, 25 },
	{ -1000, 4.5f, 16, 25 },
	{ 100000, 4.5f, 16, 25 },
	{ -100000, 4.5f, 16, 25 },
	{ 268435455, 15.5f, 1, 1 },
	{ 268435456, 0.0f, 1, 1 },
	{ 268435456, 0.5f, 16, 25 },
	{ -268435456, 0.0f, 1, 1 },
	{ -268435457, 15.5f, 16, 25 },
	{ 100000000, 4.5f, 16, 25 },
	{ -100000000, 11.75f, 16, 25 },
	{ std::numeric_limits<int32_t>::max(), 15.5f, 16, 25 },
	{ std::numeric_limits<int32_t>::min(), 0.0f, 16, 25 },
	{ std::numeric_limits<int32_t>::min(), 15.875f, 16, 25 },
	{ std::numeric_limits<int32_t>::max(), 15.5f, 65536, 1 },
	{ std::numeric_limits<int32_t>::min(), 0.0f, 65536, 1 },
	{ -1, 15.999f, 1, 20 },
	{ 0, 0.0001f, 65536, 1 },
	{ 0, 15.875f, 0, 25 },
	{ 0, 15.875f, 16, 0 },
	{ 0, 15.875f, 65537, 25 },
	{ 0, 15.875f, 16, 65537 }
} };

static constexpr float NOISE_LOCAL_TOLERANCE = 0.000001f;

/***********************************************************
* Shader Source Assembly
************************************************************/

static bool insertGpuTestModule(
	std::string& computeSource,
	const char* marker,
	const char* path)
{
	std::string moduleSource = {};

	return loadShaderSourceFile(moduleSource, path) &&
		insertShaderSource(computeSource, marker, moduleSource);
}

static bool loadGpuWideNoiseTestSource(
	std::string& computeSource)
{
	if (!loadShaderSourceFile(
		computeSource,
		"assets/shaders/tests/noise_coordinate_wide_test.comp"))
	{
		return false;
	}

	return
		insertGpuTestModule(
			computeSource,
			"// @WORLD_POSITION_IMPLEMENTATION@",
			"assets/shaders/spatial/world_position.glsl") &&
		insertGpuTestModule(
			computeSource,
			"// @WORLD_METRIC_COORDINATE_IMPLEMENTATION@",
			"assets/shaders/spatial/world_metric_coordinate.glsl") &&
		insertGpuTestModule(
			computeSource,
			"// @WIDE_INTEGER_IMPLEMENTATION@",
			"assets/shaders/math/wide_integer.glsl") &&
		insertGpuTestModule(
			computeSource,
			"// @NOISE_COORDINATE_IMPLEMENTATION@",
			"assets/shaders/math/noise_coordinates.glsl");
}

/***********************************************************
* Result Validation
************************************************************/

static bool validateWideNoiseResults(
	const std::array<WideNoiseTestOutput, TEST_INPUTS.size()>& outputs)
{
	for (std::size_t index = 0; index < TEST_INPUTS.size(); ++index)
	{
		const WideNoiseTestInput& input = TEST_INPUTS[index];
		const WideNoiseTestOutput& actual = outputs[index];

		const bool expectedValid =
			input.numerator > 0 && input.numerator <= 65536 &&
			input.denominator > 0 && input.denominator <= 65536;

		if (actual.valid != (expectedValid ? 1u : 0u))
		{
			std::printf(
				"GPU wide-noise test %zu validity mismatch: "
				"chunk=%d local=%.9f scale=%d/%d expected=%u actual=%u.\n",
				index,
				input.chunkCoordinate,
				input.localPosition,
				input.numerator,
				input.denominator,
				expectedValid ? 1u : 0u,
				actual.valid);
			return false;
		}

		if (!expectedValid)
		{
			continue;
		}

		const WorldMetricCoordinate metric = getWorldMetricCoordinate(
			input.chunkCoordinate,
			input.localPosition);

		const NoiseScale scale =
		{
			input.numerator,
			input.denominator
		};

		const NoisePosition2d expected = makeNoisePosition2d(
			metric.wholeMeters,
			0,
			metric.fractionalMeter,
			0.0f,
			scale);

		const uint64_t expectedCellBits =
			static_cast<uint64_t>(expected.cellX);
		const uint64_t actualCellBits =
			(static_cast<uint64_t>(actual.cellHigh) << 32) |
			actual.cellLow;

		float actualLocal = 0.0f;
		std::memcpy(&actualLocal, &actual.localBits, sizeof(actualLocal));

		if (actualCellBits != expectedCellBits ||
			!std::isfinite(actualLocal) ||
			std::fabs(actualLocal - expected.localX) > NOISE_LOCAL_TOLERANCE)
		{
			std::printf(
				"GPU wide-noise test %zu failed.\n"
				"  Chunk: %d  Local: %.9f  Scale: %d/%d\n"
				"  Expected cell: %lld  Local: %.9f\n"
				"  Actual bits: %08X:%08X  Local: %.9f\n",
				index,
				input.chunkCoordinate,
				input.localPosition,
				input.numerator,
				input.denominator,
				static_cast<long long>(expected.cellX),
				expected.localX,
				actual.cellHigh,
				actual.cellLow,
				actualLocal);
			return false;
		}
	}

	return true;
}

/***********************************************************
* GPU Wide Noise Validation
************************************************************/

bool validateGpuWideNoiseCoordinates()
{
	std::string computeSource = {};

	if (!loadGpuWideNoiseTestSource(computeSource))
	{
		return false;
	}

	std::array<WideNoiseTestOutput, TEST_INPUTS.size()> outputs = {};

	ShaderProgram program = {};
	GLuint buffers[2] = {};
	bool succeeded = false;

	do
	{
		if (!createComputeShaderProgram(program, computeSource.c_str()))
		{
			break;
		}

		const int testCountLocation =
			getShaderUniformLocation(program, "uTestCount");

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
			sizeof(TEST_INPUTS),
			TEST_INPUTS.data(),
			GL_STATIC_DRAW);

		glNamedBufferData(
			buffers[1],
			sizeof(outputs),
			nullptr,
			GL_DYNAMIC_READ);

		glUseProgram(program.handle);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers[0]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers[1]);
		glUniform1ui(testCountLocation, static_cast<GLuint>(TEST_INPUTS.size()));

		const GLuint groupCount =
			static_cast<GLuint>((TEST_INPUTS.size() + 63u) / 64u);

		glDispatchCompute(groupCount, 1, 1);
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		glGetNamedBufferSubData(buffers[1], 0, sizeof(outputs), outputs.data());

		succeeded = validateWideNoiseResults(outputs);
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
			"GPU wide-noise validation passed: %zu cases.\n",
			TEST_INPUTS.size());
	}

	return succeeded;
}

#endif
