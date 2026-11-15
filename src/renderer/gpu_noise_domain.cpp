///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_noise_domain.cpp
// =============================
//
// Checks every heightmap octave before GPU noise evaluation. Accepted inputs
// are safe from signed 64-bit multiplication and cell-addition overflow in
// the current two-word GLSL conversion.
//
///////////////////////////////////////////////////////////////////////////////

#include "renderer/gpu_noise_domain.h"

#include "spatial/spatial_constants.h"

#include <cmath>
#include <cstdint>
#include <limits>

/***********************************************************
* File-Local Constants
************************************************************/

// These must match the currently checked GLSL B2 contract.
static constexpr int64_t MAX_GPU_NOISE_SCALE_COMPONENT = 65536;
static constexpr uint32_t MAX_GPU_NOISE_OCTAVES = 64;

/***********************************************************
* File-Local Helpers
************************************************************/

static GpuNoiseDomainResult makeFailure(
	GpuNoiseDomainFailure failure,
	uint32_t octave)
{
	GpuNoiseDomainResult result = {};
	result.failure = failure;
	result.failedOctave = octave;
	return result;
}

static int64_t floorDividePositiveDenominator(
	int64_t numerator,
	int64_t denominator)
{
	// Requires denominator > 0. This division is always representable.
	int64_t quotient = numerator / denominator;
	const int64_t remainder = numerator % denominator;

	if (remainder < 0)
	{
		--quotient;
	}

	return quotient;
}

static bool canMultiplyInInt64(
	int64_t value,
	int64_t positiveMultiplier)
{
	const int64_t minValue = std::numeric_limits<int64_t>::min();
	const int64_t maxValue = std::numeric_limits<int64_t>::max();

	return
		positiveMultiplier > 0 &&
		value >= minValue / positiveMultiplier &&
		value <= maxValue / positiveMultiplier;
}

/***********************************************************
* GPU Noise Domain Helpers
************************************************************/

GpuNoiseDomain getFullGpuNoiseDomain()
{
	const int64_t chunkSize = static_cast<int64_t>(CHUNK_SIZE);

	GpuNoiseDomain domain = {};
	domain.minWholeMeters =
		static_cast<int64_t>(std::numeric_limits<int32_t>::min()) *
		chunkSize;
	domain.maxWholeMeters =
		static_cast<int64_t>(std::numeric_limits<int32_t>::max()) *
		chunkSize + (chunkSize - 1);

	return domain;
}

/***********************************************************
* GPU Noise Domain Validation
************************************************************/

GpuNoiseDomainResult validateGpuHeightmapNoiseDomain(
	const HeightmapDensityField& heightmap,
	const GpuNoiseDomain& domain)
{
	if (domain.minWholeMeters > domain.maxWholeMeters)
	{
		return makeFailure(GpuNoiseDomainFailure::InvalidDomain, 0);
	}

	if (!std::isfinite(heightmap.amplitude) ||
		!std::isfinite(heightmap.persistence))
	{
		return makeFailure(
			GpuNoiseDomainFailure::InvalidHeightmapParameter, 0);
	}

	if (heightmap.octaveCount == 0 ||
		heightmap.octaveCount > MAX_GPU_NOISE_OCTAVES)
	{
		return makeFailure(GpuNoiseDomainFailure::InvalidOctaveCount, 0);
	}

	int64_t numerator = heightmap.baseScale.numerator;
	int64_t denominator = heightmap.baseScale.denominator;
	const int64_t lacunarityNumerator = heightmap.lacunarity.numerator;
	const int64_t lacunarityDenominator = heightmap.lacunarity.denominator;

	if (numerator <= 0 || denominator <= 0 ||
		lacunarityNumerator <= 0 || lacunarityDenominator <= 0)
	{
		return makeFailure(GpuNoiseDomainFailure::InvalidScale, 0);
	}

	float octaveAmplitude = 1.0f;
	float totalAmplitude = 0.0f;

	for (uint32_t octave = 0; octave < heightmap.octaveCount; ++octave)
	{
		if (numerator > MAX_GPU_NOISE_SCALE_COMPONENT ||
			denominator > MAX_GPU_NOISE_SCALE_COMPONENT)
		{
			return makeFailure(
				GpuNoiseDomainFailure::ScaleComponentLimit, octave);
		}

		if (!canMultiplyInInt64(domain.minWholeMeters, numerator) ||
			!canMultiplyInInt64(domain.maxWholeMeters, numerator))
		{
			return makeFailure(
				GpuNoiseDomainFailure::WorldProductOverflow, octave);
		}

		const int64_t minProduct = domain.minWholeMeters * numerator;
		const int64_t maxProduct = domain.maxWholeMeters * numerator;

		// The local-cell offset is nonnegative. numerator + 2 is a
		// conservative upper bound that also accommodates float rounding.
		// Reject any domain too near INT64_MAX for a safe adjustment.
		const int64_t maxCell =
			floorDividePositiveDenominator(maxProduct, denominator);
		const int64_t maxOffset = numerator + 2;

		if (maxCell >
			std::numeric_limits<int64_t>::max() - maxOffset)
		{
			return makeFailure(
				GpuNoiseDomainFailure::CellRangeOverflow, octave);
		}

		// Keep a runtime finite check for the non-coordinate part of the
		// current CPU fractal accumulation contract.
		totalAmplitude += octaveAmplitude;

		if (!std::isfinite(totalAmplitude))
		{
			return makeFailure(
				GpuNoiseDomainFailure::InvalidHeightmapParameter, octave);
		}

		if (octave + 1 < heightmap.octaveCount)
		{
			octaveAmplitude *= heightmap.persistence;

			if (!std::isfinite(octaveAmplitude))
			{
				return makeFailure(
					GpuNoiseDomainFailure::InvalidHeightmapParameter, octave + 1);
			}

			if (!canMultiplyInInt64(numerator, lacunarityNumerator) ||
				!canMultiplyInInt64(denominator, lacunarityDenominator))
			{
				return makeFailure(
					GpuNoiseDomainFailure::ScaleOverflow, octave + 1);
			}

			numerator *= lacunarityNumerator;
			denominator *= lacunarityDenominator;
		}
	}

	if (!(totalAmplitude > 0.0f))
	{
		return makeFailure(
			GpuNoiseDomainFailure::InvalidHeightmapParameter,
			heightmap.octaveCount - 1);
	}

	return {};
}

/***********************************************************
* GPU Noise Domain Diagnostics
************************************************************/

const char* getGpuNoiseDomainFailureDescription(
	GpuNoiseDomainFailure failure)
{
	switch (failure)
	{
	case GpuNoiseDomainFailure::None:
		return "Supported";
	case GpuNoiseDomainFailure::InvalidDomain:
		return "Invalid whole-meter domain";
	case GpuNoiseDomainFailure::InvalidHeightmapParameter:
		return "Heightmap amplitude or persistence is not supported";
	case GpuNoiseDomainFailure::InvalidOctaveCount:
		return "Octave count is zero or exceeds the current GPU limit";
	case GpuNoiseDomainFailure::InvalidScale:
		return "Scale and lacunarity components must be positive";
	case GpuNoiseDomainFailure::ScaleComponentLimit:
		return "Octave scale component exceeds 65536";
	case GpuNoiseDomainFailure::ScaleOverflow:
		return "Octave-scale multiplication exceeds signed 64-bit range";
	case GpuNoiseDomainFailure::WorldProductOverflow:
		return "World coordinate times octave numerator exceeds signed 64-bit range";
	case GpuNoiseDomainFailure::CellRangeOverflow:
		return "Noise-cell adjustment may exceed signed 64-bit range";
	}

	return "Unknown GPU noise domain failure";
}
