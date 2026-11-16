///////////////////////////////////////////////////////////////////////////////
// math/heightmap_normals.glsl
// ===========================
//
// Generates continuous heightmap normals from CPU-prepared fractal noise.
// Input world metric coordinates need not be in the same chunk after a
// gradient offset. Base height cancels from horizontal derivatives.
//
// Requires world_metric_coordinate.glsl and fractal_noise.glsl.
//
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* World Metric Offsets
************************************************************/

// Adds a small signed metric offset without converting world meters to float.
// This handles samples crossing whole-meter and chunk boundaries.
WorldMetricCoordinate offsetGpuWorldMetricCoordinate(
	WorldMetricCoordinate coordinate,
	float offsetMeters)
{
	float total = coordinate.fractionalMeter + offsetMeters;
	int wholeOffset = int(floor(total));

	// Sign-extend the small offset to two's-complement 64-bit words.
	uvec2 delta = uvec2(
		uint(wholeOffset),
		wholeOffset < 0 ? 0xffffffffu : 0u);

	uint low = coordinate.wholeMeters.x + delta.x;
	uint carry = uint(low < coordinate.wholeMeters.x);

	WorldMetricCoordinate result;
	result.wholeMeters = uvec2(
		low,
		coordinate.wholeMeters.y + delta.y + carry);
	result.fractionalMeter = total - float(wholeOffset);

	return result;
}

/***********************************************************
* Heightmap Surface Normal Sampling
************************************************************/

// Positive density is above the surface: D = Y - H(X, Z).
// H = baseHeight + heightAmplitude * fractalNoise(X, Z).
// Base height disappears in the horizontal derivative, while dD/dY = 1.
bool trySampleGpuHeightmapNormal(
	WorldMetricCoordinate worldX,
	WorldMetricCoordinate worldZ,
	float heightAmplitude,
	float gradientStepMeters,
	out vec3 normal)
{
	normal = vec3(0.0, 1.0, 0.0);

	if (!(gradientStepMeters > 0.0) ||
		isnan(gradientStepMeters) || isinf(gradientStepMeters) ||
		isnan(heightAmplitude) || isinf(heightAmplitude))
	{
		return false;
	}

	WorldMetricCoordinate leftX =
		offsetGpuWorldMetricCoordinate(worldX, -gradientStepMeters);
	WorldMetricCoordinate rightX =
		offsetGpuWorldMetricCoordinate(worldX, gradientStepMeters);
	WorldMetricCoordinate backZ =
		offsetGpuWorldMetricCoordinate(worldZ, -gradientStepMeters);
	WorldMetricCoordinate frontZ =
		offsetGpuWorldMetricCoordinate(worldZ, gradientStepMeters);

	float left;
	float right;
	float back;
	float front;

	if (!trySampleGpuFractalValueNoise2d(leftX, worldZ, left) ||
		!trySampleGpuFractalValueNoise2d(rightX, worldZ, right) ||
		!trySampleGpuFractalValueNoise2d(worldX, backZ, back) ||
		!trySampleGpuFractalValueNoise2d(worldX, frontZ, front))
	{
		return false;
	}

	float derivativeScale =
		heightAmplitude * (0.5 / gradientStepMeters);

	normal = normalize(vec3(
		-(right - left) * derivativeScale,
		1.0,
		-(front - back) * derivativeScale));

	return true;
}
