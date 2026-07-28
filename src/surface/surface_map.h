///////////////////////////////////////////////////////////////////////////////
// surface/surface_map.h
// =====================
//
// Defines surface-crossing lookup data built from sampled chunk density data.
//
// A surface map identifies which loaded chunks contain surface-crossing voxels,
// and which voxels inside those chunks contain a density sign crossing.
//
// Surface chunks are stored contiguously by X/Z chunk column. Surface column
// records describe the corresponding ranges in SurfaceMap::chunks.
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

struct SurfaceChunkColumn
{
	int32_t chunkX = 0;
	int32_t chunkZ = 0;

	uint32_t firstSurfaceChunkIndex = 0;
	uint32_t surfaceChunkCount = 0;
};

struct SurfaceMap
{
	std::vector<SurfaceChunk> chunks = {};
	std::vector<SurfaceChunkColumn> columns = {};
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
