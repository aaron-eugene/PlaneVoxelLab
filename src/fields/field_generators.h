///////////////////////////////////////////////////////////////////////////////
// fields/field_generators.h
// =========================
//
// Declares simple density field generators used by the lab.
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

DensityField createSphereDensityField(
	const SphereDensityField& sphere);

float sampleSphereDensityField(
	const glm::dvec3& worldPosition,
	const void* userData);

/***********************************************************
* Heightmap Density Field
************************************************************/

struct HeightmapDensityField
{
	float baseHeight = 0.0;
	float amplitude = 8.0;
	float frequency = 0.05;

	uint32_t octaveCount = 5;
	float persistence = 0.5;
	float lacunarity = 2.0;

	uint32_t seed = 1337;
};

DensityField createHeightmapDensityField(
	const HeightmapDensityField& heightmap);

float sampleHeightmapDensityField(
	const glm::dvec3& worldPosition,
	const void* userData);
