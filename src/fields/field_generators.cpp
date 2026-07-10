///////////////////////////////////////////////////////////////////////////////
// fields/field_generators.cpp
// ===========================
//
// Implements simple density field generators used by the lab.
//
///////////////////////////////////////////////////////////////////////////////

#include "fields/field_generators.h"

#include <glm/geometric.hpp>

#include <cassert>
#include <cmath>

/***********************************************************
* Sphere Density Field
************************************************************/

DensityField createSphereDensityField(
	const SphereDensityField& sphere)
{
	assert(sphere.radius > 0.0);

	DensityField field = {};
	field.sample = sampleSphereDensityField;
	field.userData = &sphere;

	return field;
}

float sampleSphereDensityField(
	const glm::dvec3& worldPosition,
	const void* userData)
{
	assert(userData != nullptr);

	const SphereDensityField& sphere =
		*static_cast<const SphereDensityField*>(userData);

	const double signedDistance =
		glm::length(worldPosition - sphere.center) - sphere.radius;

	return static_cast<float>(signedDistance);
}

/***********************************************************
* Heightmap Density Field
************************************************************/

DensityField createHeightmapDensityField(
	const HeightmapDensityField& heightmap)
{
	assert(heightmap.amplitude >= 0.0);
	assert(heightmap.frequency >= 0.0);

	DensityField field = {};
	field.sample = sampleHeightmapDensityField;
	field.userData = &heightmap;

	return field;
}

float sampleHeightmapDensityField(
	const glm::dvec3& worldPosition,
	const void* userData)
{
	assert(userData != nullptr);

	const HeightmapDensityField& heightmap =
		*static_cast<const HeightmapDensityField*>(userData);

	const double terrainHeight =
		heightmap.baseHeight +
		heightmap.amplitude *
		std::sin(worldPosition.x * heightmap.frequency) *
		std::cos(worldPosition.z * heightmap.frequency);

	const double signedDistance =
		worldPosition.y - terrainHeight;

	return static_cast<float>(signedDistance);
}
