///////////////////////////////////////////////////////////////////////////////
// terrain_edit/terrain_edit.cpp
// ============================
//
// Implements chunk-local voxel excavation state.
//
///////////////////////////////////////////////////////////////////////////////

#include "terrain_edit/terrain_edit.h"
#include "spatial/spatial_coordinates.h"

#include <cstdint>
#include <utility>

/***********************************************************
* Terrain Edit Chunk Lookup
************************************************************/

TerrainEditChunk* findTerrainEditChunk(
	TerrainEdits& edits,
	const ChunkCoord& coord)
{
	for (TerrainEditChunk& editChunk : edits.chunks)
	{
		if (editChunk.coord.x == coord.x &&
			editChunk.coord.y == coord.y &&
			editChunk.coord.z == coord.z)
		{
			return &editChunk;
		}
	}

	return nullptr;
}

const TerrainEditChunk* findTerrainEditChunk(
	const TerrainEdits& edits,
	const ChunkCoord& coord)
{
	for (const TerrainEditChunk& editChunk : edits.chunks)
	{
		if (editChunk.coord.x == coord.x &&
			editChunk.coord.y == coord.y &&
			editChunk.coord.z == coord.z)
		{
			return &editChunk;
		}
	}

	return nullptr;
}

TerrainEditChunk& getOrCreateTerrainEditChunk(
	TerrainEdits& edits,
	const ChunkCoord& coord)
{
	TerrainEditChunk* editChunk =
		findTerrainEditChunk(
			edits,
			coord);

	if (editChunk != nullptr)
	{
		return *editChunk;
	}

	TerrainEditChunk newEditChunk = {};
	newEditChunk.coord = coord;

	edits.chunks.push_back(
		std::move(newEditChunk));

	return edits.chunks.back();
}

/***********************************************************
* Terrain Edit Interface
************************************************************/

bool excavateVoxel(
	TerrainEditChunk& editChunk,
	const VoxelCoord& voxelCoord)
{
	const uint32_t index =
		getVoxelIndexInChunk(voxelCoord);

	if (editChunk.voxels[index] ==
		VoxelEditState::Excavated)
	{
		return false;
	}

	editChunk.voxels[index] =
		VoxelEditState::Excavated;

	return true;
}

bool isVoxelExcavated(
	const TerrainEditChunk& editChunk,
	const VoxelCoord& voxelCoord)
{
	const uint32_t index =
		getVoxelIndexInChunk(voxelCoord);

	return editChunk.voxels[index] ==
		VoxelEditState::Excavated;
}

/***********************************************************
* Terrain Edit Lifecycle
************************************************************/

void clearTerrainEdits(
	TerrainEdits& edits)
{
	edits.chunks.clear();
}