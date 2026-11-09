///////////////////////////////////////////////////////////////////////////////
// fields/density_field.h
// ======================
//
// Declares a generic scalar density field interface.
//
// A density field maps a world-space position to a scalar value. Surface
// extraction modules interpret sign changes in this scalar field as surface
// crossings.
//
// By convention, negative density is inside solid space, positive density is
// outside solid space, and zero is on the surface.
//
// DensityField does not own userData. The caller must ensure the data object
// outlives all sampling through the DensityField.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_coordinates.h"

#include <glm/ext/vector_float3.hpp>

/***********************************************************
* Density Field Constants
************************************************************/

constexpr float DENSITY_SURFACE_VALUE = 0.0f;

/***********************************************************
* Density Field Types
************************************************************/

using DensitySampleFunction = float (*)(
	const WorldPosition& worldPosition,
	const void* userData);

struct DensityField
{
	DensitySampleFunction sample = nullptr;
	const void* userData = nullptr;
};

/***********************************************************
* Density Field Classification
************************************************************/

inline bool isDensityInside(
	float density)
{
	return density < DENSITY_SURFACE_VALUE;
}

/***********************************************************
* Density Field Gradient
************************************************************/

// Estimates the density field gradient using central differences.
// Returns an unnormalized vector, which may be zero or near zero.
// Callers must validate the gradient before normalizing it.
glm::vec3 sampleDensityFieldGradient(
	const DensityField& field,
	const WorldPosition& worldPosition,
	float sampleStepMeters);

/***********************************************************
* Density Field Interface
************************************************************/

bool isDensityFieldValid(
	const DensityField& field);

float sampleDensityField(
	const DensityField& field,
	const WorldPosition& worldPosition);
