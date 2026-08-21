///////////////////////////////////////////////////////////////////////////////
// fields/field_generators.cpp
// ===========================
//
// Implements simple density field generators used by the lab.
//
///////////////////////////////////////////////////////////////////////////////

#include "fields/field_generators.h"

#include "math/noise.h"

#include <glm/geometric.hpp>

#include <cassert>

/***********************************************************
* Sphere Density Field
************************************************************/

static float sampleSphereDensityField(
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

DensityField makeSphereDensityField(
	const SphereDensityField& sphere)
{
	assert(sphere.radius > 0.0);

	DensityField field = {};
	field.sample = sampleSphereDensityField;
	field.userData = &sphere;

	return field;
}

/***********************************************************
* Heightmap Density Field
************************************************************/

static float sampleHeightmapDensityField(
	const glm::dvec3& worldPosition,
	const void* userData)
{
	assert(userData != nullptr);

	const HeightmapDensityField& heightmap =
		*static_cast<const HeightmapDensityField*>(userData);

	const float terrainHeight =
		sampleHeightmapTerrainHeight(
			heightmap,
			static_cast<float>(worldPosition.x),
			static_cast<float>(worldPosition.z));

	const double signedHeightDelta =
		worldPosition.y - static_cast<double>(terrainHeight);

	return static_cast<float>(signedHeightDelta);
}

float sampleHeightmapTerrainHeight(
	const HeightmapDensityField& heightmap,
	float worldX,
	float worldZ)
{
	assert(heightmap.amplitude >= 0.0f);
	assert(heightmap.frequency >= 0.0f);
	assert(heightmap.octaveCount > 0);
	assert(heightmap.persistence >= 0.0f);
	assert(heightmap.lacunarity > 0.0f);

	const float sampleX =
		worldX * heightmap.frequency;

	const float sampleZ =
		worldZ * heightmap.frequency;

	const float noise =
		sampleFractalValueNoise2d(
			sampleX,
			sampleZ,
			heightmap.octaveCount,
			heightmap.persistence,
			heightmap.lacunarity,
			heightmap.seed);

	return heightmap.baseHeight +
		noise * heightmap.amplitude;
}

DensityField makeHeightmapDensityField(
	const HeightmapDensityField& heightmap)
{
	assert(heightmap.amplitude >= 0.0f);
	assert(heightmap.frequency >= 0.0f);
	assert(heightmap.octaveCount > 0);
	assert(heightmap.persistence >= 0.0f);
	assert(heightmap.lacunarity > 0.0f);

	DensityField field = {};
	field.sample = sampleHeightmapDensityField;
	field.userData = &heightmap;

	return field;
}
