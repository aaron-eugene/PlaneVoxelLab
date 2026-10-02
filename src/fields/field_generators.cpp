///////////////////////////////////////////////////////////////////////////////
// fields/field_generators.cpp
// ===========================
//
// Implements simple density field generators used by the lab.
//
///////////////////////////////////////////////////////////////////////////////

#include "fields/field_generators.h"

#include "math/noise.h"
#include "spatial/spatial_coordinates.h"

#include <glm/geometric.hpp>

#include <cassert>

/***********************************************************
* Sphere Density Field
************************************************************/

static float sampleSphereDensityField(
	const WorldPosition& worldPosition,
	const void* userData)
{
	assert(userData != nullptr);
	assert(isWorldPositionCanonical(worldPosition));

	const SphereDensityField& sphere =
		*static_cast<const SphereDensityField*>(userData);

	const int64_t chunkDeltaX =
		static_cast<int64_t>(worldPosition.chunk.x) -
		static_cast<int64_t>(sphere.center.chunk.x);

	const int64_t chunkDeltaY =
		static_cast<int64_t>(worldPosition.chunk.y) -
		static_cast<int64_t>(sphere.center.chunk.y);

	const int64_t chunkDeltaZ =
		static_cast<int64_t>(worldPosition.chunk.z) -
		static_cast<int64_t>(sphere.center.chunk.z);

	const glm::vec3 offset =
	{
		static_cast<float>(chunkDeltaX) *
			CHUNK_SIZE_METERS +
			worldPosition.localPosition.x -
			sphere.center.localPosition.x,

		static_cast<float>(chunkDeltaY) *
			CHUNK_SIZE_METERS +
			worldPosition.localPosition.y -
			sphere.center.localPosition.y,

		static_cast<float>(chunkDeltaZ) *
			CHUNK_SIZE_METERS +
			worldPosition.localPosition.z -
			sphere.center.localPosition.z
	};

	return glm::length(offset) -
		sphere.radius;
}

DensityField makeSphereDensityField(
	const SphereDensityField& sphere)
{
	assert(isWorldPositionCanonical(sphere.center));
	assert(sphere.radius > 0.0f);

	DensityField field = {};
	field.sample = sampleSphereDensityField;
	field.userData = &sphere;

	return field;
}

/***********************************************************
* Heightmap Density Field
************************************************************/

static float sampleHeightmapDensityField(
	const WorldPosition& worldPosition,
	const void* userData)
{
	assert(userData != nullptr);

	const HeightmapDensityField& heightmap =
		*static_cast<const HeightmapDensityField*>(
			userData);

	const WorldMetricCoordinate terrainHeight =
		sampleHeightmapTerrainHeight(
			heightmap,
			worldPosition);

	const WorldMetricCoordinate worldY =
		getWorldMetricCoordinate(
			worldPosition.chunk.y,
			worldPosition.localPosition.y);

	return getWorldMetricCoordinateOffset(
		terrainHeight,
		worldY);
}

WorldMetricCoordinate sampleHeightmapTerrainHeight(
	const HeightmapDensityField& heightmap,
	const WorldPosition& worldPosition)
{
	assert(isWorldPositionCanonical(worldPosition));

	assert(heightmap.amplitude >= 0.0f);

	assert(heightmap.baseScale.numerator > 0);
	assert(heightmap.baseScale.denominator > 0);

	assert(heightmap.octaveCount > 0);
	assert(heightmap.persistence >= 0.0f);

	assert(heightmap.lacunarity.numerator > 0);
	assert(heightmap.lacunarity.denominator > 0);

	const WorldMetricCoordinate worldX =
		getWorldMetricCoordinate(
			worldPosition.chunk.x,
			worldPosition.localPosition.x);

	const WorldMetricCoordinate worldZ =
		getWorldMetricCoordinate(
			worldPosition.chunk.z,
			worldPosition.localPosition.z);

	const float noise =
		sampleFractalValueNoise2d(
			worldX.wholeMeters,
			worldZ.wholeMeters,
			worldX.fractionalMeter,
			worldZ.fractionalMeter,
			heightmap.baseScale,
			heightmap.octaveCount,
			heightmap.persistence,
			heightmap.lacunarity,
			heightmap.seed);

	const float heightOffset =
		noise *
		heightmap.amplitude;

	return offsetWorldMetricCoordinate(
		heightmap.baseHeight,
		heightOffset);
}

DensityField makeHeightmapDensityField(
	const HeightmapDensityField& heightmap)
{
	assert(heightmap.amplitude >= 0.0f);

	assert(heightmap.baseScale.numerator > 0);
	assert(heightmap.baseScale.denominator > 0);

	assert(heightmap.octaveCount > 0);
	assert(heightmap.persistence >= 0.0f);

	assert(heightmap.lacunarity.numerator > 0);
	assert(heightmap.lacunarity.denominator > 0);

	DensityField field = {};
	field.sample = sampleHeightmapDensityField;
	field.userData = &heightmap;

	return field;
}
