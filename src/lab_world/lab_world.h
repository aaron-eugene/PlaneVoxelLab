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
#include "surface_map/surface_map.h"

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
	//--------------------------------------------------
	// Density Configuration
	//--------------------------------------------------
	LabWorldDensityFieldType activeDensityFieldType =
		LabWorldDensityFieldType::Sphere;
	
	SphereDensityField sphereField = {};
	HeightmapDensityField heightmapField = {};

	DensityField densityField = {};

	//--------------------------------------------------
	// Primary Generated Data
	//--------------------------------------------------
	std::vector<Chunk> chunks = {};

	//--------------------------------------------------
	// Derived World Data
	//--------------------------------------------------
	SurfaceMap surfaceMap = {};
};

/***********************************************************
* Lab World Lifecycle
************************************************************/

void initializeLabWorld(LabWorld& world);

void shutdownLabWorld(LabWorld& world);

/***********************************************************
* Lab World Density Field
************************************************************/

void setLabWorldDensityFieldType(
	LabWorld& world,
	LabWorldDensityFieldType fieldType);

void rebuildLabWorldDensityData(
	LabWorld& world);
