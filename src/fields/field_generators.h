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
	double baseHeight = 0.0;
	double amplitude = 8.0;
	double frequency = 0.05;

	uint32_t octaveCount = 5;
	double persistence = 0.5;
	double lacunarity = 2.0;

	uint32_t seed = 1337;
};

DensityField createHeightmapDensityField(
	const HeightmapDensityField& heightmap);

float sampleHeightmapDensityField(
	const glm::dvec3& worldPosition,
	const void* userData);
