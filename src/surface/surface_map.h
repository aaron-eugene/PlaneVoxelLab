///////////////////////////////////////////////////////////////////////////////
// surface/surface_map.h
// =====================
//
// Defines surface-crossing lookup data built from sampled chunk density data.
//
// A surface map identifies which loaded chunks contain surface-crossing voxels,
// and which voxels inside those chunks contain a density sign crossing.
//
// This module does not own chunks, density fields, render resources, or active
// experiment state.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "lab_world/lab_world_coordinates.h"

#include <cstdint>
#include <vector>

struct Chunk;

/***********************************************************
* Surface Map Types
************************************************************/

struct SurfaceVoxel
{
	VoxelCoord coord = {};
};

struct SurfaceChunk
{
	uint32_t chunkIndex = 0;
	ChunkCoord coord = {};

	std::vector<SurfaceVoxel> voxels = {};
};

struct SurfaceMap
{
	std::vector<SurfaceChunk> chunks = {};
};

/***********************************************************
* Surface Map Lifecycle
************************************************************/

void clearSurfaceMap(
	SurfaceMap& surfaceMap);

/***********************************************************
* Surface Map Rebuild
************************************************************/

bool rebuildSurfaceMap(
	SurfaceMap& surfaceMap,
	const std::vector<Chunk>& chunks);
