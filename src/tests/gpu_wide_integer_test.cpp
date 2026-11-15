///////////////////////////////////////////////////////////////////////////////
// tests/gpu_wide_integer_test.cpp
// ===============================
//
// Compares two-word GPU multiplication and floor division against C++.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests/gpu_wide_integer_test.h"

#ifndef NDEBUG

#include "renderer/shader.h"

#include <glad/glad.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>

/***********************************************************
* Test Types
************************************************************/

struct WideTestCase
{
	int64_t value = 0;
	uint32_t multiplier = 1;
	uint32_t divisor = 1;
};

struct WideTestInput
{
	uint32_t low = 0;
	uint32_t high = 0;
	uint32_t multiplier = 1;
	uint32_t divisor = 1;
};

struct alignas(16) WideTestOutput
{
	uint32_t productLow = 0;
	uint32_t productHigh = 0;
	uint32_t productValid = 0;
	uint32_t reserved = 0;
	uint32_t quotientLow = 0;
	uint32_t quotientHigh = 0;
	uint32_t remainder = 0;
	uint32_t divisionValid = 0;
};

static_assert(sizeof(WideTestInput) == 16);
static_assert(sizeof(WideTestOutput) == 32);

/***********************************************************
* Test Data
************************************************************/

static constexpr std::array<WideTestCase, 20> TEST_CASES =
{ {
	{ 0, 1, 20 },
	{ 1, 1, 20 },
	{ -1, 1, 20 },
	{ 19, 2, 20 },
	{ -19, 2, 20 },
	{ 20, 16, 20 },
	{ -20, 16, 20 },
	{ 21, 16, 20 },
	{ -21, 16, 20 },
	{ 4294967295LL, 16, 25 },
	{ -4294967295LL, 16, 25 },
	{ 4294967296LL, 65536, 65536 },
	{ -4294967296LL, 65536, 65536 },
	{ std::numeric_limits<int64_t>::max(), 1, 1 },
	{ std::numeric_limits<int64_t>::min(), 1, 1 },
	{ std::numeric_limits<int64_t>::max(), 2, 25 },
	{ std::numeric_limits<int64_t>::min(), 2, 25 },
	{ -1, 0, 20 },
	{ 1, 1, 0 },
	{ -1, 1, 0x80000000u }
} };

/***********************************************************
* CPU Reference Helpers
************************************************************/

static uint64_t getMagnitude(
	int64_t value)
{
	if (value >= 0)
	{
		return static_cast<uint64_t>(value);
	}

	// This also handles INT64_MIN without signed negation overflow.
	return static_cast<uint64_t>(-(value + 1)) + 1u;
}

static uint64_t combineWords(
	uint32_t low,
	uint32_t high)
{
	return (static_cast<uint64_t>(high) << 32) | low;
}

static bool validateWideResults(
	const std::array<WideTestOutput, TEST_CASES.size()>& outputs)
{
	for (std::size_t index = 0; index < TEST_CASES.size(); ++index)
	{
		const WideTestCase& test = TEST_CASES[index];
		const WideTestOutput& actual = outputs[index];

		const uint64_t magnitude = getMagnitude(test.value);
		const uint64_t maxMagnitude = test.value < 0
			? (uint64_t{ 1 } << 63)
			: static_cast<uint64_t>(std::numeric_limits<int64_t>::max());

		const bool multiplyValid = test.multiplier != 0 &&
			magnitude <= maxMagnitude / test.multiplier;

		if (actual.productValid != (multiplyValid ? 1u : 0u))
		{
			std::printf("Wide multiplication validity failed at case %zu.\n", index);
			return false;
		}

		if (multiplyValid)
		{
			const uint64_t productMagnitude = magnitude * test.multiplier;
			const uint64_t expectedBits = test.value < 0
				? uint64_t{ 0 } - productMagnitude
				: productMagnitude;

			if (combineWords(actual.productLow, actual.productHigh) != expectedBits)
			{
				std::printf("Wide multiplication value failed at case %zu.\n", index);
				return false;
			}
		}

		const bool divideValid = test.divisor != 0 &&
			test.divisor <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max());

		if (actual.divisionValid != (divideValid ? 1u : 0u))
		{
			std::printf("Wide division validity failed at case %zu.\n", index);
			return false;
		}

		if (divideValid)
		{
			const int64_t divisor = static_cast<int64_t>(test.divisor);
			int64_t quotient = test.value / divisor;
			int64_t remainder = test.value % divisor;

			if (remainder < 0)
			{
				--quotient;
				remainder += divisor;
			}

			if (combineWords(actual.quotientLow, actual.quotientHigh) !=
				static_cast<uint64_t>(quotient) ||
				actual.remainder != static_cast<uint32_t>(remainder))
			{
				std::printf(
					"Wide floor division failed at case %zu: value=%lld divisor=%u.\n",
					index,
					static_cast<long long>(test.value),
					test.divisor);
				return false;
			}
		}
	}

	return true;
}

/***********************************************************
* GPU Test Execution
************************************************************/

bool validateGpuWideIntegerArithmetic()
{
	std::string computeSource = {};

	if (!loadShaderSourceFile(
		computeSource,
		"assets/shaders/tests/wide_integer_test.comp"))
	{
		return false;
	}

	std::string wideSource = {};

	if (!loadShaderSourceFile(
		wideSource,
		"assets/shaders/math/wide_integer.glsl") ||
		!insertShaderSource(
			computeSource,
			"// @WIDE_INTEGER_IMPLEMENTATION@",
			wideSource))
	{
		return false;
	}

	std::array<WideTestInput, TEST_CASES.size()> inputs = {};
	std::array<WideTestOutput, TEST_CASES.size()> outputs = {};

	for (std::size_t index = 0; index < TEST_CASES.size(); ++index)
	{
		const WideTestCase& test = TEST_CASES[index];
		const uint64_t bits = static_cast<uint64_t>(test.value);

		inputs[index].low = static_cast<uint32_t>(bits);
		inputs[index].high = static_cast<uint32_t>(bits >> 32);
		inputs[index].multiplier = test.multiplier;
		inputs[index].divisor = test.divisor;
	}

	ShaderProgram program = {};
	GLuint buffers[2] = {};
	bool succeeded = false;

	do
	{
		if (!createComputeShaderProgram(program, computeSource.c_str()))
		{
			break;
		}

		const int testCountLocation = getShaderUniformLocation(program, "uTestCount");

		if (testCountLocation < 0)
		{
			break;
		}

		glCreateBuffers(2, buffers);

		if (buffers[0] == 0 || buffers[1] == 0)
		{
			break;
		}

		glNamedBufferData(buffers[0], sizeof(inputs), inputs.data(), GL_STATIC_DRAW);
		glNamedBufferData(buffers[1], sizeof(outputs), nullptr, GL_DYNAMIC_READ);

		glUseProgram(program.handle);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffers[0]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffers[1]);
		glUniform1ui(testCountLocation, static_cast<GLuint>(TEST_CASES.size()));

		const GLuint groupCount =
			static_cast<GLuint>((TEST_CASES.size() + 63u) / 64u);

		glDispatchCompute(groupCount, 1, 1);
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		glGetNamedBufferSubData(buffers[1], 0, sizeof(outputs), outputs.data());

		succeeded = validateWideResults(outputs);
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
		std::printf("GPU wide-integer validation passed: %zu cases.\n", TEST_CASES.size());
	}

	return succeeded;
}

#endif
