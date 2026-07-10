///////////////////////////////////////////////////////////////////////////////
// lab_world.h
// ===========
//
// Defines the shared world data consumed by the surface reference and active
// terrain experiments.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "chunk/chunk.h"
#include "fields/density_field.h"
#include "fields/field_generators.h"
#include "surface/surface_map.h"

#include <vector>

/***********************************************************
* Lab World Field Selection
************************************************************/

enum class LabWorldDensityFieldType
{
	Sphere,
	Heightmap,
};

/***********************************************************
* Lab World State
************************************************************/

struct LabWorld
{
	LabWorldDensityFieldType activeDensityFieldType =
		LabWorldDensityFieldType::Sphere;
	
	SphereDensityField sphereField = {};
	HeightmapDensityField heightmapField = {};

	DensityField densityField = {};

	std::vector<Chunk> chunks = {};
	SurfaceMap surfaceMap = {};
};

/***********************************************************
* Lab World Lifecycle
************************************************************/

bool initializeLabWorld(LabWorld& world);

void shutdownLabWorld(LabWorld& world);

/***********************************************************
* Lab World Density Field
************************************************************/

void setLabWorldDensityFieldType(
	LabWorld& world,
	LabWorldDensityFieldType fieldType);

bool rebuildLabWorldDensityData(
	LabWorld& world);
