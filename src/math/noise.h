///////////////////////////////////////////////////////////////////////////////
// math/noise.h
// ============
//
// Declares deterministic procedural noise helpers.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Value Noise
************************************************************/

float sampleValueNoise2d(
	float x,
	float z,
	uint32_t seed);

/***********************************************************
* Fractal Noise
************************************************************/

float sampleFractalValueNoise2d(
	float x,
	float z,
	uint32_t octaveCount,
	float persistence,
	float lacunarity,
	uint32_t seed);
