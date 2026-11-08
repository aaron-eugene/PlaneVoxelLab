///////////////////////////////////////////////////////////////////////////////
// lab_world/lab_world.h
// =====================
//
// Defines shared procedural terrain configuration, loaded chunk data,
// and derived surface lookup data for the lab.
//
// LabWorld coordinates density sampling and surface discovery but does
// not own experiment-specific geometry or render resources.
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
		LabWorldDensityFieldType::Heightmap;
	
	SphereDensityField sphereField = {};
	HeightmapDensityField heightmapField = {};

	// Borrows the active field configuration stored above.
	// Copying or moving LabWorld requires rebinding this interface.
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

/***********************************************************
* Lab World Chunk Lookup
************************************************************/

Chunk* findLabWorldChunk(
	LabWorld& world,
	const ChunkCoord& coord);

const Chunk* findLabWorldChunk(
	const LabWorld& world,
	const ChunkCoord& coord);
