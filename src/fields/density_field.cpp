///////////////////////////////////////////////////////////////////////////////
// fields/density_field.cpp
// ========================
//
// Implements validation and sampling for the generic density-field interface.
//
///////////////////////////////////////////////////////////////////////////////

#include "fields/density_field.h"

#include <cassert>

/***********************************************************
* Density Field Interface
************************************************************/

bool isDensityFieldValid(
	const DensityField& field)
{
	return field.sample != nullptr;
}

float sampleDensityField(
	const DensityField& field,
	const WorldPosition& worldPosition)
{
	assert(isDensityFieldValid(field));
	assert(isWorldPositionCanonical(worldPosition));

	return field.sample(
		worldPosition,
		field.userData);
}
