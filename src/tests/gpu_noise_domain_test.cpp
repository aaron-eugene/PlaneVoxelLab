///////////////////////////////////////////////////////////////////////////////
// tests/gpu_noise_domain_test.cpp
// ===============================
//
// Exercises the CPU-side GPU noise-domain go/no-go contract.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests/gpu_noise_domain_test.h"

#ifndef NDEBUG

#include "renderer/gpu_noise_domain.h"

#include <cstdint>
#include <cstdio>
#include <limits>

/***********************************************************
* Test Helpers
************************************************************/

static bool expectGpuNoiseDomainResult(
	const char* testName,
	const HeightmapDensityField& heightmap,
	const GpuNoiseDomain& domain,
	GpuNoiseDomainFailure expectedFailure,
	uint32_t expectedOctave)
{
	const GpuNoiseDomainResult result =
		validateGpuHeightmapNoiseDomain(heightmap, domain);

	if (result.failure != expectedFailure ||
		result.failedOctave != expectedOctave)
	{
		std::printf(
			"GPU noise-domain test '%s' failed: %s (octave %u).\n",
			testName,
			getGpuNoiseDomainFailureDescription(result.failure),
			result.failedOctave);
		return false;
	}

	return true;
}

/***********************************************************
* GPU Noise Domain Tests
************************************************************/

bool validateGpuNoiseDomainConfiguration()
{
	HeightmapDensityField field = {};
	field.baseScale = { 1, 25 };
	field.lacunarity = { 2, 1 };
	field.octaveCount = 5;
	field.persistence = 0.5f;
	field.amplitude = 22.0f;

	const GpuNoiseDomain fullDomain = getFullGpuNoiseDomain();
	uint32_t passed = 0;

	if (!expectGpuNoiseDomainResult(
		"current heightmap", field, fullDomain,
		GpuNoiseDomainFailure::None, 0)) return false;
	++passed;

	field.octaveCount = 17;
	if (!expectGpuNoiseDomainResult(
		"largest accepted octave scale", field, fullDomain,
		GpuNoiseDomainFailure::None, 0)) return false;
	++passed;

	field.octaveCount = 18;
	if (!expectGpuNoiseDomainResult(
		"next octave exceeds scale limit", field, fullDomain,
		GpuNoiseDomainFailure::ScaleComponentLimit, 17)) return false;
	++passed;

	field.octaveCount = 0;
	if (!expectGpuNoiseDomainResult(
		"zero octaves", field, fullDomain,
		GpuNoiseDomainFailure::InvalidOctaveCount, 0)) return false;
	++passed;

	field.octaveCount = 5;
	field.baseScale = { 0, 25 };
	if (!expectGpuNoiseDomainResult(
		"zero numerator", field, fullDomain,
		GpuNoiseDomainFailure::InvalidScale, 0)) return false;
	++passed;

	field.baseScale = { 1, 65537 };
	if (!expectGpuNoiseDomainResult(
		"large denominator", field, fullDomain,
		GpuNoiseDomainFailure::ScaleComponentLimit, 0)) return false;
	++passed;

	field.baseScale = { 1, 25 };
	field.lacunarity = { 2, 2 };
	if (!expectGpuNoiseDomainResult(
		"denominator growth", field, fullDomain,
		GpuNoiseDomainFailure::None, 0)) return false;
	++passed;

	field.lacunarity = { 2, 1 };
	field.octaveCount = 1;
	field.baseScale = { 2, 1 };
	const GpuNoiseDomain largeDomain =
	{
		std::numeric_limits<int64_t>::min(),
		std::numeric_limits<int64_t>::max()
	};
	if (!expectGpuNoiseDomainResult(
		"wide product overflow", field, largeDomain,
		GpuNoiseDomainFailure::WorldProductOverflow, 0)) return false;
	++passed;

	field.baseScale = { 1, 1 };
	const GpuNoiseDomain nearMaxDomain =
	{
		std::numeric_limits<int64_t>::max() - 1,
		std::numeric_limits<int64_t>::max()
	};
	if (!expectGpuNoiseDomainResult(
		"cell adjustment near maximum", field, nearMaxDomain,
		GpuNoiseDomainFailure::CellRangeOverflow, 0)) return false;
	++passed;

	const GpuNoiseDomain invalidDomain = { 10, -10 };
	if (!expectGpuNoiseDomainResult(
		"reversed domain", field, invalidDomain,
		GpuNoiseDomainFailure::InvalidDomain, 0)) return false;
	++passed;

	std::printf(
		"GPU noise-domain configuration validation passed: %u cases.\n",
		passed);

	return true;
}

#endif
