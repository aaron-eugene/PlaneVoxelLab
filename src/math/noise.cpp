///////////////////////////////////////////////////////////////////////////////
// math/noise.cpp
// ==============
//
// Implements deterministic procedural value noise helpers.
//
///////////////////////////////////////////////////////////////////////////////

#include "math/noise.h"

#include "math/math_utils.h"

#include <cmath>
#include <cstdint>

/***********************************************************
* File-Local Helpers
************************************************************/

static uint32_t hashUint(
	uint32_t value)
{
	value ^= value >> 16;
	value *= 0x7feb352d;
	value ^= value >> 15;
	value *= 0x846ca68b;
	value ^= value >> 16;

	return value;
}

static uint32_t hashGridCoord(
	int32_t x,
	int32_t z,
	uint32_t seed)
{
	uint32_t hash = seed;

	hash ^= hashUint(static_cast<uint32_t>(x) + 0x9e3779b9);
	hash = hashUint(hash);

	hash ^= hashUint(static_cast<uint32_t>(z) + 0x85ebca6b);
	hash = hashUint(hash);

	return hash;
}

static float getGridRandomValue(
	int32_t x,
	int32_t z,
	uint32_t seed)
{
	const uint32_t hash =
		hashGridCoord(
			x,
			z,
			seed);

	const float normalized =
		static_cast<float>(hash) /
		static_cast<float>(UINT32_MAX);

	return normalized * 2.0f - 1.0f;
}

/***********************************************************
* Value Noise
************************************************************/

float sampleValueNoise2d(
	float x,
	float z,
	uint32_t seed)
{
	const int32_t x0 =
		static_cast<int32_t>(std::floor(x));

	const int32_t z0 =
		static_cast<int32_t>(std::floor(z));

	const int32_t x1 = x0 + 1;
	const int32_t z1 = z0 + 1;

	const float localX =
		x - static_cast<float>(x0);

	const float localZ =
		z - static_cast<float>(z0);

	const float smoothX =
		fadeSmoothstep5(localX);

	const float smoothZ =
		fadeSmoothstep5(localZ);

	const float value00 =
		getGridRandomValue(
			x0,
			z0,
			seed);

	const float value10 =
		getGridRandomValue(
			x1,
			z0,
			seed);

	const float value01 =
		getGridRandomValue(
			x0,
			z1,
			seed);

	const float value11 =
		getGridRandomValue(
			x1,
			z1,
			seed);

	const float valueX0 =
		lerpFloat(
			value00,
			value10,
			smoothX);

	const float valueX1 =
		lerpFloat(
			value01,
			value11,
			smoothX);

	return lerpFloat(
		valueX0,
		valueX1,
		smoothZ);
}

/***********************************************************
* Fractal Noise
************************************************************/

float sampleFractalValueNoise2d(
	float x,
	float z,
	uint32_t octaveCount,
	float persistence,
	float lacunarity,
	uint32_t seed)
{
	float total = 0.0f;
	float amplitude = 1.0f;
	float frequency = 1.0f;
	float maxAmplitude = 0.0f;

	for (uint32_t octaveIndex = 0;
		octaveIndex < octaveCount;
		++octaveIndex)
	{
		total +=
			sampleValueNoise2d(
				x * frequency,
				z * frequency,
				seed + octaveIndex * 1013) *
			amplitude;

		maxAmplitude += amplitude;

		amplitude *= persistence;
		frequency *= lacunarity;
	}

	if (maxAmplitude <= 0.0f)
	{
		return 0.0f;
	}

	return total / maxAmplitude;
}
