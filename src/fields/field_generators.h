///////////////////////////////////////////////////////////////////////////////
// fields/field_generators.h
// =========================
//
// Declares simple procedural density-field generators.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "fields/density_field.h"
#include "math/noise.h"
#include "spatial/spatial_coordinates.h"

#include <cstdint>

/***********************************************************
* Sphere Density Field
************************************************************/

struct SphereDensityField
{
	WorldPosition center = {};
	float radius = 1.0f;
};

DensityField makeSphereDensityField(
	const SphereDensityField& sphere);

/***********************************************************
* Heightmap Density Field
************************************************************/

struct HeightmapDensityField
{
	WorldMetricCoordinate baseHeight = {};
	
	float amplitude = 8.0f;

	NoiseScale baseScale =
	{
		1,
		20
	};

	uint32_t octaveCount = 5;
	float persistence = 0.5f;

	NoiseScale lacunarity =
	{
		2,
		1
	};

	uint32_t seed = 9109;
};

WorldMetricCoordinate sampleHeightmapTerrainHeight(
	const HeightmapDensityField& heightmap,
	const WorldPosition& worldPosition);

DensityField makeHeightmapDensityField(
	const HeightmapDensityField& heightmap);
