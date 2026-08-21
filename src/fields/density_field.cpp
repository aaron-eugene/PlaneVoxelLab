///////////////////////////////////////////////////////////////////////////////
// fields/density_field.cpp
// ========================
//
// Implements validation and sampling for the generic density-field interface.
//
///////////////////////////////////////////////////////////////////////////////

#include "fields/density_field.h"

#include <glm/ext/vector_double3.hpp>

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
	const glm::dvec3& worldPosition)
{
	assert(isDensityFieldValid(field));

	return field.sample(
		worldPosition,
		field.userData);
}
