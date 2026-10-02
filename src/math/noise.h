///////////////////////////////////////////////////////////////////////////////
// math/noise.h
// ============
//
// Declares deterministic procedural noise helpers.
// 
// Noise scales use modest positive integer ratios and practical terrain 
// octave counts. Intermediate scale arithmetic uses int64_t. Overflow beyond 
// that supported range is a programmer/configuration error.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Noise Types
************************************************************/

struct NoisePosition2d
{
	int64_t cellX = 0;
	int64_t cellZ = 0;

	float localX = 0.0f;
	float localZ = 0.0f;
};

// numerator / denominator = noise cells per meter
struct NoiseScale
{
	int64_t numerator = 1;
	int64_t denominator = 1;
};

/***********************************************************
* Noise Position Construction
************************************************************/

NoisePosition2d makeNoisePosition2d(
	int64_t wholeMeterX,
	int64_t wholeMeterZ,
	float fractionalMeterX,
	float fractionalMeterZ,
	const NoiseScale& scale);

/***********************************************************
* Value Noise
************************************************************/

float sampleValueNoise2d(
	const NoisePosition2d& position,
	uint32_t seed);

/***********************************************************
* Fractal Noise
************************************************************/

float sampleFractalValueNoise2d(
	int64_t wholeMeterX,
	int64_t wholeMeterZ,
	float fractionalMeterX,
	float fractionalMeterZ,
	const NoiseScale& baseScale,
	uint32_t octaveCount,
	float persistence,
	const NoiseScale& lacunarity,
	uint32_t seed);
