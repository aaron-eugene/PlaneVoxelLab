///////////////////////////////////////////////////////////////////////////////
// tests/gpu_world_metric_test.cpp
// ===============================
//
// Validates GPU world-metric conversion and bounded 32-bit noise-coordinate
// conversion against the existing CPU reference implementations.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests/gpu_world_metric_test.h"

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

struct WorldMetricTestInput
{
	int32_t chunkCoordinate = 0;
	float localPosition = 0.0f;
};

struct alignas(16) WorldMetricTestOutput
{
	uint32_t low = 0;
	uint32_t high = 0;
	uint32_t fractionalBits = 0;
	uint32_t reserved = 0;
};

struct NoiseCoordinateTestInput
{
	int32_t chunkCoordinate = 0;
	float localPosition = 0.0f;
	int32_t numerator = 1;
	int32_t denominator = 1;
};

struct alignas(16) NoiseCoordinateTestOutput
{
	int32_t cell = 0;
	int32_t localBits = 0;
	int32_t valid = 0;
	int32_t reserved = 0;
};

static_assert(sizeof(WorldMetricTestInput) == 8);
static_assert(sizeof(WorldMetricTestOutput) == 16);
static_assert(sizeof(NoiseCoordinateTestInput) == 16);
static_assert(sizeof(NoiseCoordinateTestOutput) == 16);

/***********************************************************
* Test Data
************************************************************/

static constexpr std::array<WorldMetricTestInput, 17> WORLD_METRIC_INPUTS =
{ {
	{ 0, 0.0f },
	{ 0, 15.5f },
	{ 1, 0.25f },
	{ -1, 15.25f },
	{ -1, 0.0f },
	{ -2, 0.125f },
	{ 1, 15.875f },
	{ 268435455, 15.5f },
	{ 268435456, 0.0f },
	{ 268435456, 0.5f },
	{ -268435456, 0.0f },
	{ -268435457, 15.5f },
	{ 100000000, 4.5f },
	{ -100000000, 11.75f },
	{ std::numeric_limits<int32_t>::max(), 15.5f },
	{ std::numeric_limits<int32_t>::min(), 0.0f },
	{ std::numeric_limits<int32_t>::min(), 15.875f }
} };

static constexpr std::array<NoiseCoordinateTestInput, 16> NOISE_COORDINATE_INPUTS =
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
	{ std::numeric_limits<int32_t>::max(), 15.5f, 16, 25 },
	{ std::numeric_limits<int32_t>::min(), 0.0f, 16, 25 }
} };

static constexpr float NOISE_LOCAL_TOLERANCE = 0.000001f;
static constexpr int32_t MAX_BOUNDED_SCALE_COMPONENT = 65536;

/***********************************************************
* Test Shader Helpers
************************************************************/

static bool loadGpuWorldMetricTestSource(
	std::string& computeSource)
{
	if (!loadShaderSourceFile(
		computeSource,
		"assets/shaders/tests/world_metric_coordinate_test.comp"))
	{
		return false;
	}

	std::string worldPositionSource = {};

	if (!loadShaderSourceFile(
		worldPositionSource,
		"assets/shaders/spatial/world_position.glsl"))
	{
		return false;
	}

	if (!insertShaderSource(
		computeSource,
		"// @WORLD_POSITION_IMPLEMENTATION@",
		worldPositionSource))
	{
		return false;
	}

	std::string metricSource = {};

	if (!loadShaderSourceFile(
		metricSource,
		"assets/shaders/spatial/world_metric_coordinate.glsl"))
	{
		return false;
	}

	return insertShaderSource(
		computeSource,
		"// @WORLD_METRIC_COORDINATE_IMPLEMENTATION@",
		metricSource);
}

static bool loadGpuNoiseCoordinateTestSource(
	std::string& computeSource)
{
	if (!loadShaderSourceFile(
		computeSource,
		"assets/shaders/tests/noise_coordinate_test.comp"))
	{
		return false;
	}

	std::string worldPositionSource = {};

	if (!loadShaderSourceFile(
		worldPositionSource,
		"assets/shaders/spatial/world_position.glsl"))
	{
		return false;
	}

	if (!insertShaderSource(
		computeSource,
		"// @WORLD_POSITION_IMPLEMENTATION@",
		worldPositionSource))
	{
		return false;
	}

	std::string metricSource = {};

	if (!loadShaderSourceFile(
		metricSource,
		"assets/shaders/spatial/world_metric_coordinate.glsl"))
	{
		return false;
	}

	if (!insertShaderSource(
		computeSource,
		"// @WORLD_METRIC_COORDINATE_IMPLEMENTATION@",
		metricSource))
	{
		return false;
	}

	std::string wideIntegerSource = {};

	if (!loadShaderSourceFile(
		wideIntegerSource,
		"assets/shaders/math/wide_integer.glsl"))
	{
		return false;
	}

	if (!insertShaderSource(
		computeSource,
		"// @WIDE_INTEGER_IMPLEMENTATION@",
		wideIntegerSource))
	{
		return false;
	}

	std::string noiseSource = {};

	if (!loadShaderSourceFile(
		noiseSource,
		"assets/shaders/math/noise_coordinates.glsl"))
	{
		return false;
	}

	return insertShaderSource(
		computeSource,
		"// @NOISE_COORDINATE_IMPLEMENTATION@",
		noiseSource);
}

/***********************************************************
* GPU Test Execution
************************************************************/

static bool runGpuCoordinateTest(
	const std::string& computeSource,
	const void* inputs,
	std::size_t inputBytes,
	void* outputs,
	std::size_t outputBytes,
	uint32_t testCount)
{
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
			static_cast<GLsizeiptr>(inputBytes),
			inputs,
			GL_STATIC_DRAW);

		glNamedBufferData(
			buffers[1],
			static_cast<GLsizeiptr>(outputBytes),
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
			testCount);

		const GLuint groupCount =
			(testCount + 63u) / 64u;

		glDispatchCompute(groupCount, 1, 1);

		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

		glGetNamedBufferSubData(
			buffers[1],
			0,
			static_cast<GLsizeiptr>(outputBytes),
			outputs);

		succeeded = true;
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

	return succeeded;
}

/***********************************************************
* World Metric Result Validation
************************************************************/

static bool validateWorldMetricResults(
	const std::array<WorldMetricTestOutput, WORLD_METRIC_INPUTS.size()>& outputs)
{
	for (std::size_t index = 0;
		index < WORLD_METRIC_INPUTS.size();
		++index)
	{
		const WorldMetricTestInput& input =
			WORLD_METRIC_INPUTS[index];

		const WorldMetricCoordinate expected =
			getWorldMetricCoordinate(
				input.chunkCoordinate,
				input.localPosition);

		const uint64_t expectedBits =
			static_cast<uint64_t>(expected.wholeMeters);

		const uint32_t expectedLow =
			static_cast<uint32_t>(expectedBits);

		const uint32_t expectedHigh =
			static_cast<uint32_t>(expectedBits >> 32);

		uint32_t expectedFractionBits = 0;

		std::memcpy(
			&expectedFractionBits,
			&expected.fractionalMeter,
			sizeof(expectedFractionBits));

		const WorldMetricTestOutput& actual = outputs[index];

		if (actual.low != expectedLow ||
			actual.high != expectedHigh ||
			actual.fractionalBits != expectedFractionBits)
		{
			std::printf(
				"GPU world-metric test %zu failed.\n"
				"  Chunk: %d  Local: %.6f\n"
				"  Expected: %08X:%08X  Fraction: %08X\n"
				"  Actual:   %08X:%08X  Fraction: %08X\n",
				index,
				input.chunkCoordinate,
				input.localPosition,
				expectedHigh,
				expectedLow,
				expectedFractionBits,
				actual.high,
				actual.low,
				actual.fractionalBits);

			return false;
		}
	}

	return true;
}

/***********************************************************
* Bounded Noise Result Validation
************************************************************/

static bool isNoiseCoordinate32Supported(
	const NoiseCoordinateTestInput& input)
{
	if (input.numerator <= 0 ||
		input.denominator <= 0 ||
		input.numerator > MAX_BOUNDED_SCALE_COMPONENT ||
		input.denominator > MAX_BOUNDED_SCALE_COMPONENT)
	{
		return false;
	}

	const WorldMetricCoordinate metric =
		getWorldMetricCoordinate(
			input.chunkCoordinate,
			input.localPosition);

	const int64_t minValue =
		std::numeric_limits<int32_t>::min();

	const int64_t maxValue =
		std::numeric_limits<int32_t>::max();

	if (metric.wholeMeters < minValue ||
		metric.wholeMeters > maxValue)
	{
		return false;
	}

	const int64_t product =
		metric.wholeMeters *
		static_cast<int64_t>(input.numerator);

	if (product < minValue || product > maxValue)
	{
		return false;
	}

	// Signed floor division matches the CPU noise-coordinate semantics.
	int64_t cell = product / input.denominator;
	int64_t remainder = product % input.denominator;

	if (remainder < 0)
	{
		--cell;
		remainder += input.denominator;
	}

	const float local =
		(static_cast<float>(remainder) +
			metric.fractionalMeter *
			static_cast<float>(input.numerator)) /
		static_cast<float>(input.denominator);

	const int64_t localCellOffset =
		static_cast<int64_t>(std::floor(local));

	return cell + localCellOffset >= minValue &&
		cell + localCellOffset <= maxValue;
}

static bool validateNoiseCoordinateResults(
	const std::array<NoiseCoordinateTestOutput, NOISE_COORDINATE_INPUTS.size()>& outputs)
{
	for (std::size_t index = 0;
		index < NOISE_COORDINATE_INPUTS.size();
		++index)
	{
		const NoiseCoordinateTestInput& input =
			NOISE_COORDINATE_INPUTS[index];

		const bool expectedValid =
			isNoiseCoordinate32Supported(input);

		const NoiseCoordinateTestOutput& actual = outputs[index];

		if (actual.valid != (expectedValid ? 1 : 0))
		{
			std::printf(
				"GPU bounded-noise test %zu validity mismatch.\n"
				"  Chunk: %d  Local: %.6f  Scale: %d/%d\n"
				"  Expected valid: %d  Actual valid: %d\n",
				index,
				input.chunkCoordinate,
				input.localPosition,
				input.numerator,
				input.denominator,
				expectedValid ? 1 : 0,
				actual.valid);

			return false;
		}

		if (!expectedValid)
		{
			continue;
		}

		const WorldMetricCoordinate metric =
			getWorldMetricCoordinate(
				input.chunkCoordinate,
				input.localPosition);

		const NoiseScale scale =
		{
			input.numerator,
			input.denominator
		};

		const NoisePosition2d expected =
			makeNoisePosition2d(
				metric.wholeMeters,
				0,
				metric.fractionalMeter,
				0.0f,
				scale);

		float actualLocal = 0.0f;

		std::memcpy(
			&actualLocal,
			&actual.localBits,
			sizeof(actualLocal));

		if (actual.cell != expected.cellX ||
			!std::isfinite(actualLocal) ||
			std::fabs(actualLocal - expected.localX) >
			NOISE_LOCAL_TOLERANCE)
		{
			std::printf(
				"GPU bounded-noise test %zu failed.\n"
				"  Chunk: %d  Local: %.6f  Scale: %d/%d\n"
				"  Expected cell: %lld  Local: %.9f\n"
				"  Actual cell:   %d  Local: %.9f\n",
				index,
				input.chunkCoordinate,
				input.localPosition,
				input.numerator,
				input.denominator,
				static_cast<long long>(expected.cellX),
				expected.localX,
				actual.cell,
				actualLocal);

			return false;
		}
	}

	return true;
}

/***********************************************************
* GPU Coordinate Validation
************************************************************/

bool validateGpuWorldMetricCoordinates()
{
	std::string worldMetricSource = {};

	if (!loadGpuWorldMetricTestSource(worldMetricSource))
	{
		return false;
	}

	std::array<WorldMetricTestOutput, WORLD_METRIC_INPUTS.size()> metricOutputs = {};

	if (!runGpuCoordinateTest(
		worldMetricSource,
		WORLD_METRIC_INPUTS.data(),
		sizeof(WORLD_METRIC_INPUTS),
		metricOutputs.data(),
		sizeof(metricOutputs),
		static_cast<uint32_t>(WORLD_METRIC_INPUTS.size())))
	{
		return false;
	}

	if (!validateWorldMetricResults(metricOutputs))
	{
		return false;
	}

	std::printf(
		"GPU world-metric validation passed: %zu cases.\n",
		WORLD_METRIC_INPUTS.size());

	std::string noiseSource = {};

	if (!loadGpuNoiseCoordinateTestSource(noiseSource))
	{
		return false;
	}

	std::array<NoiseCoordinateTestOutput, NOISE_COORDINATE_INPUTS.size()> noiseOutputs = {};

	if (!runGpuCoordinateTest(
		noiseSource,
		NOISE_COORDINATE_INPUTS.data(),
		sizeof(NOISE_COORDINATE_INPUTS),
		noiseOutputs.data(),
		sizeof(noiseOutputs),
		static_cast<uint32_t>(NOISE_COORDINATE_INPUTS.size())))
	{
		return false;
	}

	if (!validateNoiseCoordinateResults(noiseOutputs))
	{
		return false;
	}

	std::printf(
		"GPU bounded-noise validation passed: %zu cases.\n",
		NOISE_COORDINATE_INPUTS.size());

	return true;
}

#endif
