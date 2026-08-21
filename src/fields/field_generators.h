///////////////////////////////////////////////////////////////////////////////
// fields/field_generators.h
// =========================
//
// Declares simple procedural density-field generators.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "fields/density_field.h"

#include <glm/ext/vector_double3.hpp>

#include <cstdint>

/***********************************************************
* Sphere Density Field
************************************************************/

struct SphereDensityField
{
	glm::dvec3 center = {};
	double radius = 1.0;
};

DensityField makeSphereDensityField(
	const SphereDensityField& sphere);

/***********************************************************
* Heightmap Density Field
************************************************************/

struct HeightmapDensityField
{
	float baseHeight = 0.0f;
	float amplitude = 8.0f;
	float frequency = 0.05f;

	uint32_t octaveCount = 5;
	float persistence = 0.5f;
	float lacunarity = 2.0f;

	uint32_t seed = 1337;
};

float sampleHeightmapTerrainHeight(
	const HeightmapDensityField& heightmap,
	float worldX,
	float worldZ);

DensityField makeHeightmapDensityField(
	const HeightmapDensityField& heightmap);
