///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_noise_domain.h
// ===========================
//
// Declares CPU validation and preparation of GPU heightmap noise parameters.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "fields/field_generators.h"
#include "spatial/spatial_coordinates.h"

#include <array>
#include <cstdint>

/***********************************************************
* GPU Noise Domain Types
************************************************************/

// Inclusive whole-meter bounds for X/Z samples. Fractional meters are [0, 1).
struct GpuNoiseDomain
{
	int64_t minWholeMeters = 0;
	int64_t maxWholeMeters = 0;
};

enum class GpuNoiseDomainFailure
{
	None,
	InvalidDomain,
	InvalidHeightmapParameter,
	InvalidOctaveCount,
	InvalidScale,
	ScaleComponentLimit,
	ScaleOverflow,
	WorldProductOverflow,
	CellRangeOverflow
};

struct GpuNoiseDomainResult
{
	GpuNoiseDomainFailure failure = GpuNoiseDomainFailure::None;
	uint32_t failedOctave = 0;
};

/***********************************************************
* Prepared GPU Heightmap Types
************************************************************/

static constexpr uint32_t MAX_GPU_NOISE_OCTAVES = 64;

struct GpuNoiseOctave
{
	int32_t numerator = 1;
	int32_t denominator = 1;
	float amplitude = 1.0f;
};

struct GpuHeightmapNoiseConfiguration
{
	std::array<GpuNoiseOctave, MAX_GPU_NOISE_OCTAVES> octaves = {};

	uint32_t octaveCount = 0;
	uint32_t seed = 0;

	float totalAmplitude = 0.0f;
	float heightAmplitude = 0.0f;

	WorldMetricCoordinate baseHeight = {};
};

/***********************************************************
* GPU Noise Domain Validation
************************************************************/

// Covers canonical whole-meter positions across the full signed int32
// chunk-coordinate range. Gradient offsets require separate consideration.
GpuNoiseDomain getFullGpuNoiseDomain();

// Checks octave scales and integer arithmetic against the current B2 GLSL
// contract. Does not establish safe WorldPosition normalization or
// cross-GPU floating-point equivalence.
GpuNoiseDomainResult validateGpuHeightmapNoiseDomain(
	const HeightmapDensityField& heightmap,
	const GpuNoiseDomain& domain);

/***********************************************************
* GPU Heightmap Configuration Preparation
************************************************************/

// Resets the output on failure. On success, stores the validated effective
// octave scales and weights for subsequent GPU upload.
GpuNoiseDomainResult prepareGpuHeightmapNoiseConfiguration(
	const HeightmapDensityField& heightmap,
	const GpuNoiseDomain& domain,
	GpuHeightmapNoiseConfiguration& configuration);

/***********************************************************
* GPU Noise Domain Diagnostics
************************************************************/

const char* getGpuNoiseDomainFailureDescription(
	GpuNoiseDomainFailure failure);
