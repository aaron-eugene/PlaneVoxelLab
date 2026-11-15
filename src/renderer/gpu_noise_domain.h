///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_noise_domain.h
// ===========================
//
// Declares CPU validation of the numerical domain supported by GPU heightmap
// noise-coordinate conversion.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "fields/field_generators.h"

#include <cstdint>

/***********************************************************
* GPU Noise Domain Types
************************************************************/

// Inclusive whole-meter bounds for every X/Z coordinate that the GPU
// noise implementation may sample. Fractional meters are in [0, 1).
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
* GPU Noise Domain Validation
************************************************************/

// Covers canonical whole-meter positions in the complete signed int32
// chunk-coordinate range. Gradient offsets must be included separately
// if they can reach beyond this domain.
GpuNoiseDomain getFullGpuNoiseDomain();

// Validates all effective octave scales against the current B2 GLSL
// implementation. Does not validate GPU WorldPosition normalization,
// heightmap vertical positioning, or cross-GPU floating-point equality.
GpuNoiseDomainResult validateGpuHeightmapNoiseDomain(
	const HeightmapDensityField& heightmap,
	const GpuNoiseDomain& domain);

const char* getGpuNoiseDomainFailureDescription(
	GpuNoiseDomainFailure failure);
