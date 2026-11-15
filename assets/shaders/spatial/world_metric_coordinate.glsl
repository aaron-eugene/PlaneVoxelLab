
///////////////////////////////////////////////////////////////////////////////
// spatial/world_metric_coordinate.glsl
// ====================================
//
// Converts chunk/local coordinates into whole-meter and fractional-meter
// coordinates without losing large-world integer precision.
//
// Whole meters use two 32-bit words in signed two's-complement form.
// No native 64-bit GPU integer support is required.
//
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* Metric Coordinate Types
************************************************************/

struct WorldMetricCoordinate
{
	uvec2 wholeMeters;
	float fractionalMeter;
};

/***********************************************************
* Wide Integer Helpers
************************************************************/

// Adds a nonnegative 32-bit value to a two-word integer.
uvec2 addWideInteger(
	uvec2 value,
	uint offset)
{
	uint low =
		value.x + offset;

	uint carry =
		uint(low < value.x);

	return uvec2(
		low,
		value.y + carry);
}

// Multiplies a signed chunk coordinate by a positive
// unsigned chunk size, preserving the signed 64-bit result.
uvec2 multiplyChunkCoordinate(
	int chunkCoordinate,
	uint chunkSize)
{
	uint high = 0u;
	uint low = 0u;

	umulExtended(
		uint(chunkCoordinate),
		chunkSize,
		high,
		low);

	// Correct the upper word for the signed operand.
	if (chunkCoordinate < 0)
	{
		high -= chunkSize;
	}

	return uvec2(low, high);
}

/***********************************************************
* Metric Coordinate Conversion
************************************************************/

WorldMetricCoordinate getWorldMetricCoordinate(
	int chunkCoordinate,
	float localPosition)
{
	int localWholeMeters =
		int(floor(localPosition));

	uvec2 wholeMeters =
		multiplyChunkCoordinate(
			chunkCoordinate,
			uint(CHUNK_SIZE_METERS));

	wholeMeters =
		addWideInteger(
			wholeMeters,
			uint(localWholeMeters));

	WorldMetricCoordinate result;

	result.wholeMeters = wholeMeters;
	result.fractionalMeter =
		localPosition -
		float(localWholeMeters);

	return result;
}
