
///////////////////////////////////////////////////////////////////////////////
// math/value_noise.glsl
// =====================
//
// Implements deterministic 2D value noise using wide integer lattice
// coordinates.
//
// Requires math/noise_coordinates.glsl for NoiseCoordinate64.
//
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* Noise Hashing
************************************************************/

uint hashNoiseUint(
	uint value)
{
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;

	return value;
}

uint hashNoiseInt64(
	uvec2 value)
{
	uint hash =
		hashNoiseUint(value.x);

	hash ^=
		hashNoiseUint(
			value.y + 0x9e3779b9u);

	return hashNoiseUint(hash);
}

uint hashNoiseGridCoord(
	uvec2 x,
	uvec2 z,
	uint seed)
{
	uint hash = seed;

	hash ^=
		hashNoiseInt64(x);

	hash =
		hashNoiseUint(hash);

	hash ^=
		hashNoiseInt64(z);

	hash =
		hashNoiseUint(hash);

	return hash;
}

float getNoiseGridRandomValue(
	uvec2 x,
	uvec2 z,
	uint seed)
{
	uint hash =
		hashNoiseGridCoord(
			x,
			z,
			seed);

	float normalized =
		float(hash) /
		float(0xffffffffu);

	return normalized * 2.0 - 1.0;
}

/***********************************************************
* Wide Lattice Coordinate Helpers
************************************************************/

uvec2 incrementNoiseCell(
	uvec2 cell)
{
	uint low = cell.x + 1u;

	return uvec2(
		low,
		cell.y + uint(low == 0u));
}

/***********************************************************
* Noise Interpolation
************************************************************/

float lerpNoiseFloat(
	float a,
	float b,
	float t)
{
	return a + (b - a) * t;
}

float fadeNoiseSmoothstep5(
	float value)
{
	return value *
		value *
		value *
		(value * (value * 6.0 - 15.0) + 10.0);
}

/***********************************************************
* Value Noise
************************************************************/

float sampleGpuValueNoise2d(
	NoiseCoordinate64 x,
	NoiseCoordinate64 z,
	uint seed)
{
	uvec2 x0 = x.cell;
	uvec2 z0 = z.cell;

	uvec2 x1 = incrementNoiseCell(x0);
	uvec2 z1 = incrementNoiseCell(z0);

	float smoothX =
		fadeNoiseSmoothstep5(x.local);

	float smoothZ =
		fadeNoiseSmoothstep5(z.local);

	float value00 =
		getNoiseGridRandomValue(
			x0, z0, seed);

	float value10 =
		getNoiseGridRandomValue(
			x1, z0, seed);

	float value01 =
		getNoiseGridRandomValue(
			x0, z1, seed);

	float value11 =
		getNoiseGridRandomValue(
			x1, z1, seed);

	float valueX0 =
		lerpNoiseFloat(
			value00,
			value10,
			smoothX);

	float valueX1 =
		lerpNoiseFloat(
			value01,
			value11,
			smoothX);

	return lerpNoiseFloat(
		valueX0,
		valueX1,
		smoothZ);
}

