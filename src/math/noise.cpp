///////////////////////////////////////////////////////////////////////////////
// math/noise.cpp
// ==============
//
// Implements deterministic procedural value noise helpers.
//
///////////////////////////////////////////////////////////////////////////////

#include "math/noise.h"

#include "math/math_utils.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

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

static uint32_t hashInt64(
	int64_t value)
{
	const uint64_t unsignedValue =
		static_cast<uint64_t>(
			value);

	const uint32_t low =
		static_cast<uint32_t>(
			unsignedValue);

	const uint32_t high =
		static_cast<uint32_t>(
			unsignedValue >> 32);

	uint32_t hash =
		hashUint(low);

	hash ^=
		hashUint(
			high +
			0x9e3779b9);

	return hashUint(hash);
}

static uint32_t hashGridCoord(
	int64_t x,
	int64_t z,
	uint32_t seed)
{
	uint32_t hash = seed;

	hash ^=
		hashInt64(x);

	hash =
		hashUint(hash);

	hash ^=
		hashInt64(z);

	hash =
		hashUint(hash);

	return hash;
}

static float getGridRandomValue(
	int64_t x,
	int64_t z,
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

static bool canMultiplyPositiveInt64(
	int64_t a,
	int64_t b)
{
	assert(a > 0);
	assert(b > 0);

	return
		a <=
		std::numeric_limits<int64_t>::max() /
		b;
}

static bool canMultiplyInt64ByPositive(
	int64_t value,
	int64_t multiplier)
{
	assert(multiplier > 0);

	if (value > 0)
	{
		return
			value <=
			std::numeric_limits<int64_t>::max() /
			multiplier;
	}

	if (value < 0)
	{
		return
			value >=
			std::numeric_limits<int64_t>::min() /
			multiplier;
	}

	return true;
}

static int64_t floorDivide(
	int64_t numerator,
	int64_t denominator)
{
	assert(denominator > 0);

	int64_t quotient =
		numerator /
		denominator;

	const int64_t remainder =
		numerator %
		denominator;

	if (remainder < 0)
	{
		--quotient;
	}

	return quotient;
}

static void getScaledNoiseCoordinate(
	int64_t wholeMeters,
	float fractionalMeter,
	const NoiseScale& scale,
	int64_t& cell,
	float& local)
{
	assert(fractionalMeter >= 0.0f);
	assert(fractionalMeter < 1.0f);

	assert(scale.numerator > 0);
	assert(scale.denominator > 0);

	assert(
		canMultiplyInt64ByPositive(
			wholeMeters,
			scale.numerator));

	const int64_t scaledWholeMeters =
		wholeMeters *
		scale.numerator;

	cell =
		floorDivide(
			scaledWholeMeters,
			scale.denominator);

	int64_t remainder =
		scaledWholeMeters %
		scale.denominator;

	if (remainder < 0)
	{
		remainder +=
			scale.denominator;
	}

	local =
		(static_cast<float>(remainder) +
			fractionalMeter *
			static_cast<float>(
				scale.numerator)) /
		static_cast<float>(
			scale.denominator);

	const int64_t localCellOffset =
		static_cast<int64_t>(
			std::floor(local));

	cell +=
		localCellOffset;

	local -=
		static_cast<float>(
			localCellOffset);

	assert(local >= 0.0f);
	assert(local < 1.0f);
}

/***********************************************************
* Noise Position Construction
************************************************************/

NoisePosition2d makeNoisePosition2d(
	int64_t wholeMeterX,
	int64_t wholeMeterZ,
	float fractionalMeterX,
	float fractionalMeterZ,
	const NoiseScale& scale)
{
	assert(fractionalMeterX >= 0.0f);
	assert(fractionalMeterX < 1.0f);
	assert(fractionalMeterZ >= 0.0f);
	assert(fractionalMeterZ < 1.0f);

	assert(scale.numerator > 0);
	assert(scale.denominator > 0);

	NoisePosition2d position = {};

	getScaledNoiseCoordinate(
		wholeMeterX,
		fractionalMeterX,
		scale,
		position.cellX,
		position.localX);

	getScaledNoiseCoordinate(
		wholeMeterZ,
		fractionalMeterZ,
		scale,
		position.cellZ,
		position.localZ);

	return position;
}

/***********************************************************
* Value Noise
************************************************************/

float sampleValueNoise2d(
	const NoisePosition2d& position,
	uint32_t seed)
{
	assert(position.localX >= 0.0f);
	assert(position.localX < 1.0f);
	assert(position.localZ >= 0.0f);
	assert(position.localZ < 1.0f);

	const int64_t x0 =
		position.cellX;

	const int64_t z0 =
		position.cellZ;

	const int64_t x1 =
		x0 + 1;

	const int64_t z1 =
		z0 + 1;

	const float smoothX =
		fadeSmoothstep5(
			position.localX);

	const float smoothZ =
		fadeSmoothstep5(
			position.localZ);

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
	int64_t wholeMeterX,
	int64_t wholeMeterZ,
	float fractionalMeterX,
	float fractionalMeterZ,
	const NoiseScale& baseScale,
	uint32_t octaveCount,
	float persistence,
	const NoiseScale& lacunarity,
	uint32_t seed)
{
	assert(octaveCount > 0);
	assert(persistence >= 0.0f);

	assert(baseScale.numerator > 0);
	assert(baseScale.denominator > 0);

	assert(lacunarity.numerator > 0);
	assert(lacunarity.denominator > 0);

	float total = 0.0f;
	float octaveAmplitude = 1.0f;
	float maxAmplitude = 0.0f;

	NoiseScale octaveScale =
		baseScale;

	for (uint32_t octaveIndex = 0;
		octaveIndex < octaveCount;
		++octaveIndex)
	{
		const NoisePosition2d position =
			makeNoisePosition2d(
				wholeMeterX,
				wholeMeterZ,
				fractionalMeterX,
				fractionalMeterZ,
				octaveScale);

		total +=
			sampleValueNoise2d(
				position,
				seed +
				octaveIndex * 1013) *
			octaveAmplitude;

		maxAmplitude +=
			octaveAmplitude;

		octaveAmplitude *=
			persistence;

		if (octaveIndex + 1 < octaveCount)
		{
			assert(
				canMultiplyPositiveInt64(
					octaveScale.numerator,
					lacunarity.numerator));

			octaveScale.numerator *=
				lacunarity.numerator;

			assert(
				canMultiplyPositiveInt64(
					octaveScale.denominator,
					lacunarity.denominator));

			octaveScale.denominator *=
				lacunarity.denominator;
		}
	}

	return total /
		maxAmplitude;
}
