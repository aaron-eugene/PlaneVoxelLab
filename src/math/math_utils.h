///////////////////////////////////////////////////////////////////////////////
// math/math_utils.h
// =================
//
// Declares small general-purpose math helpers.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

/***********************************************************
* Interpolation
************************************************************/

inline float lerpFloat(
	float a,
	float b,
	float t)
{
	return a + (b - a) * t;
}

inline float fadeSmoothstep5(
	float value)
{
	return value *
		value *
		value *
		(value * (value * 6.0f - 15.0f) + 10.0f);
}
