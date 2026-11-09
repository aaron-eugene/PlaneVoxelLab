///////////////////////////////////////////////////////////////////////////////
// fields/density_field.cpp
// ========================
//
// Implements validation and sampling for the generic density-field interface.
//
///////////////////////////////////////////////////////////////////////////////

#include "fields/density_field.h"

#include <glm/ext/vector_float3.hpp>

#include <cassert>
#include <cstdint>

/***********************************************************
* Density Field Gradient
************************************************************/

glm::vec3 sampleDensityFieldGradient(
	const DensityField& field,
	const WorldPosition& worldPosition,
	float sampleStepMeters)
{
	assert(isDensityFieldValid(field));
	assert(isWorldPositionCanonical(worldPosition));
	assert(sampleStepMeters > 0.0f);

	glm::vec3 gradient = {};

	for (uint32_t axis = 0;
		axis < 3;
		++axis)
	{
		glm::vec3 offset = {};
		offset[axis] = sampleStepMeters;

		const WorldPosition positivePosition =
			makeWorldPosition(
				worldPosition.chunk,
				worldPosition.localPosition + offset);

		const WorldPosition negativePosition =
			makeWorldPosition(
				worldPosition.chunk,
				worldPosition.localPosition - offset);

		const float positiveDensity =
			sampleDensityField(
				field,
				positivePosition);

		const float negativeDensity =
			sampleDensityField(
				field,
				negativePosition);

		gradient[axis] =
			(positiveDensity - negativeDensity) /
			(2.0f * sampleStepMeters);
	}

	return gradient;
}

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
