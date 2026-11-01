///////////////////////////////////////////////////////////////////////////////
// terrain_edit/terrain_edit.h
// ==========================
//
// Defines chunk-local voxel excavation state.
//
// Excavated voxels contain no solid material, regardless of the original
// density field. This module does not generate or own surface geometry.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"

#include <array>
#include <cstdint>
#include <vector>

/***********************************************************
* Terrain Edit Types
************************************************************/

enum class VoxelEditState : uint8_t
{
	Unmodified,
	Excavated
};

struct TerrainEditChunk
{
	ChunkCoord coord = {};

	std::array<VoxelEditState, CHUNK_VOXEL_COUNT> voxels = {};
};

struct TerrainEdits
{
	std::vector<TerrainEditChunk> chunks = {};
};

/***********************************************************
* Terrain Edit Chunk Lookup
************************************************************/

TerrainEditChunk* findTerrainEditChunk(
	TerrainEdits& edits,
	const ChunkCoord& coord);

const TerrainEditChunk* findTerrainEditChunk(
	const TerrainEdits& edits,
	const ChunkCoord& coord);

TerrainEditChunk& getOrCreateTerrainEditChunk(
	TerrainEdits& edits,
	const ChunkCoord& coord);

/***********************************************************
* Terrain Edit Interface
************************************************************/

bool excavateVoxel(
	TerrainEditChunk& editChunk,
	const VoxelCoord& voxelCoord);

bool isVoxelExcavated(
	const TerrainEditChunk& editChunk,
	const VoxelCoord& voxelCoord);

/***********************************************************
* Terrain Edit Lifecycle
************************************************************/

void clearTerrainEdits(
	TerrainEdits& edits);