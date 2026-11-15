///////////////////////////////////////////////////////////////////////////////
// math/wide_integer.glsl
// ======================
//
// Minimal signed 64-bit arithmetic using two 32-bit unsigned words.
// The low word is x; the high word is y. Signed values use two's complement.
//
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* Wide Integer Helpers
************************************************************/

uvec2 negateWideInteger(
	uvec2 value)
{
	uvec2 inverted = ~value;
	uint low = inverted.x + 1u;
	uint carry = uint(low == 0u);

	return uvec2(low, inverted.y + carry);
}

bool isWideIntegerNegative(
	uvec2 value)
{
	return (value.y & 0x80000000u) != 0u;
}

bool isWideIntegerZero(
	uvec2 value)
{
	return value.x == 0u && value.y == 0u;
}

/***********************************************************
* Signed Wide Multiplication
************************************************************/

// Multiplies a signed 64-bit value by a positive 32-bit value.
// Returns false if the mathematical result is outside int64 range.
bool tryMultiplyWideInteger(
	uvec2 value,
	uint multiplier,
	out uvec2 result)
{
	result = uvec2(0u);

	if (multiplier == 0u)
	{
		return false;
	}

	bool negative = isWideIntegerNegative(value);
	uvec2 magnitude = negative ? negateWideInteger(value) : value;

	uint lowHigh = 0u;
	uint lowLow = 0u;
	uint highHigh = 0u;
	uint highLow = 0u;

	umulExtended(magnitude.x, multiplier, lowHigh, lowLow);
	umulExtended(magnitude.y, multiplier, highHigh, highLow);

	uint productHigh = lowHigh + highLow;
	bool carry = productHigh < lowHigh;

	if (highHigh != 0u || carry)
	{
		return false;
	}

	// Positive results cannot set bit 63; negative magnitudes
	// may reach 2^63 (the magnitude of INT64_MIN).
	uint highLimit = negative ? 0x80000000u : 0x7fffffffu;

	if (productHigh > highLimit ||
		(negative && productHigh == 0x80000000u && lowLow != 0u))
	{
		return false;
	}

	uvec2 product = uvec2(lowLow, productHigh);
	result = negative ? negateWideInteger(product) : product;

	return true;
}

/***********************************************************
* Unsigned Wide Division
************************************************************/

// Divides an unsigned 64-bit value by a positive integer <= INT32_MAX.
// The remainder stays within 32 bits during binary long division.
void divideUnsignedWideInteger(
	uvec2 dividend,
	uint divisor,
	out uvec2 quotient,
	out uint remainder)
{
	quotient = uvec2(0u);
	remainder = 0u;

	for (int bit = 63; bit >= 0; --bit)
	{
		uint currentBit = bit >= 32
			? (dividend.y >> uint(bit - 32)) & 1u
			: (dividend.x >> uint(bit)) & 1u;

		remainder = (remainder << 1) | currentBit;

		if (remainder >= divisor)
		{
			remainder -= divisor;

			if (bit >= 32)
			{
				quotient.y |= 1u << uint(bit - 32);
			}
			else
			{
				quotient.x |= 1u << uint(bit);
			}
		}
	}
}

/***********************************************************
* Signed Wide Floor Division
************************************************************/

// Returns floor(value / divisor), with remainder in [0, divisor).
// Divisor is limited to [1, INT32_MAX] so remainder doubling
// in unsigned binary long division cannot overflow a uint.
bool tryDivideWideIntegerFloor(
	uvec2 value,
	uint divisor,
	out uvec2 quotient,
	out uint remainder)
{
	quotient = uvec2(0u);
	remainder = 0u;

	if (divisor == 0u || divisor > 0x7fffffffu)
	{
		return false;
	}

	bool negative = isWideIntegerNegative(value);
	uvec2 magnitude = negative ? negateWideInteger(value) : value;

	divideUnsignedWideInteger(magnitude, divisor, quotient, remainder);

	if (negative)
	{
		quotient = negateWideInteger(quotient);

		if (remainder != 0u)
		{
			// Floor quotient = -trunc(magnitude / divisor) - 1.
			uint low = quotient.x - 1u;
			uint borrow = uint(quotient.x == 0u);
			quotient = uvec2(low, quotient.y - borrow);
			remainder = divisor - remainder;
		}
	}

	return true;
}
