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

#include <glm/ext/vector_double3.hpp>

/***********************************************************
* Density Field Types
************************************************************/

typedef float (*DensitySampleFunction)(
	const glm::dvec3& worldPosition,
	const void* userData);

struct DensityField
{
	DensitySampleFunction sample = nullptr;
	const void* userData = nullptr;
};

/***********************************************************
* Density Field Interface
************************************************************/

bool isDensityFieldValid(
	const DensityField& field);

float sampleDensityField(
	const DensityField& field,
	const glm::dvec3& worldPosition);
