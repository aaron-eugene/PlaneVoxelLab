///////////////////////////////////////////////////////////////////////////////
// math/noise_coordinates.glsl
// ===========================
// Converts world-metric coordinates into procedural noise-cell coordinates.
//
// B1 uses bounded 32-bit arithmetic. B2 preserves signed 64-bit cells
// using two unsigned 32-bit words.
//
// Requires world_metric_coordinate.glsl and wide_integer.glsl for B2.
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* Noise Coordinate Types
************************************************************/

struct NoiseCoordinate32
{
	int cell;
	float local;
};

/***********************************************************
* Bounded Noise Coordinate Conversion
************************************************************/

bool tryGetScaledNoiseCoordinate32(
	WorldMetricCoordinate coordinate,
	int numerator,
	int denominator,
	out NoiseCoordinate32 result)
{
	result.cell = 0;
	result.local = 0.0;

	if (numerator <= 0 || denominator <= 0 ||
		numerator > 65536 || denominator > 65536)
	{
		return false;
	}

	uint lowWord = coordinate.wholeMeters.x;
	uint highWord = coordinate.wholeMeters.y;

	uint expectedHigh =
		(lowWord & 0x80000000u) != 0u
			? 0xffffffffu
			: 0u;

	if (highWord != expectedHigh)
	{
		return false;
	}

	int wholeMeters = int(lowWord);

	int productHigh = 0;
	int productLow = 0;

	imulExtended(
		wholeMeters,
		numerator,
		productHigh,
		productLow);

	int expectedProductHigh = productLow < 0 ? -1 : 0;

	if (productHigh != expectedProductHigh)
	{
		return false;
	}

	int scaledWholeMeters = productLow;

	int cell = scaledWholeMeters / denominator;

	// GLSL does not define signed remainder for negative operands.
	int remainder = scaledWholeMeters - cell * denominator;

	if (remainder < 0)
	{
		--cell;
		remainder += denominator;
	}

	float local =
		(float(remainder) +
			coordinate.fractionalMeter * float(numerator)) /
		float(denominator);

	int localCellOffset = int(floor(local));

	if (localCellOffset > 0 &&
		cell > 2147483647 - localCellOffset)
	{
		return false;
	}

	cell += localCellOffset;
	local -= float(localCellOffset);

	result.cell = cell;
	result.local = local;

	return true;
}

/***********************************************************
* Wide Noise Coordinate Types
************************************************************/

struct NoiseCoordinate64
{
	uvec2 cell;
	float local;
};

/***********************************************************
* Wide Noise Coordinate Conversion
************************************************************/

bool tryGetScaledNoiseCoordinate64(
	WorldMetricCoordinate coordinate,
	int numerator,
	int denominator,
	out NoiseCoordinate64 result)
{
	result.cell = uvec2(0u);
	result.local = 0.0;

	// Temporary test limits, matching B1.
	if (numerator <= 0 || denominator <= 0 ||
		numerator > 65536 || denominator > 65536)
	{
		return false;
	}

	if (isnan(coordinate.fractionalMeter) ||
		isinf(coordinate.fractionalMeter) ||
		coordinate.fractionalMeter < 0.0 ||
		coordinate.fractionalMeter >= 1.0)
	{
		return false;
	}

	uvec2 scaledWholeMeters;

	if (!tryMultiplyWideInteger(
		coordinate.wholeMeters,
		uint(numerator),
		scaledWholeMeters))
	{
		return false;
	}

	uvec2 cell;
	uint remainder;

	if (!tryDivideWideIntegerFloor(
		scaledWholeMeters,
		uint(denominator),
		cell,
		remainder))
	{
		return false;
	}

	// Avoid converting an absolute world position to float.
	float local =
		(float(remainder) +
			coordinate.fractionalMeter * float(numerator)) /
		float(denominator);

	uint localCellOffset = uint(floor(local));
	uvec2 adjustedCell = addWideInteger(cell, localCellOffset);

	// A nonnegative cell cannot wrap into the negative int64 range.
	if (!isWideIntegerNegative(cell) &&
		isWideIntegerNegative(adjustedCell))
	{
		return false;
	}

	result.cell = adjustedCell;
	result.local = local - float(localCellOffset);

	return true;
}
