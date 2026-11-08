///////////////////////////////////////////////////////////////////////////////
// surface_map/surface_map.cpp
// ===========================
//
// Implements XZ column organization of discovered surface voxels.
//
///////////////////////////////////////////////////////////////////////////////

#include "surface_map/surface_map.h"

#include "spatial/spatial_coordinates.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

/***********************************************************
* Local Helpers
************************************************************/

static bool isChunkCoordBeforeInSurfaceMapOrder(
	const ChunkCoord& coordA,
	const ChunkCoord& coordB)
{
	if (coordA.x != coordB.x)
	{
		return coordA.x < coordB.x;
	}

	if (coordA.z != coordB.z)
	{
		return coordA.z < coordB.z;
	}

	return coordA.y < coordB.y;
}

static bool isSurfaceChunkInColumn(
	const DiscoveredSurfaceChunk& surfaceChunk,
	const SurfaceChunkColumn& column)
{
	return
		surfaceChunk.coord.x ==
		column.chunkX &&
		surfaceChunk.coord.z ==
		column.chunkZ;
}

static bool isDiscoveredSurfaceChunkBefore(
	const DiscoveredSurfaceChunk& chunkA,
	const DiscoveredSurfaceChunk& chunkB)
{
	if (isChunkCoordBeforeInSurfaceMapOrder(
		chunkA.coord,
		chunkB.coord))
	{
		return true;
	}

	if (isChunkCoordBeforeInSurfaceMapOrder(
		chunkB.coord,
		chunkA.coord))
	{
		return false;
	}

	return chunkA.chunkIndex < chunkB.chunkIndex;
}

/***********************************************************
* Surface Map Lifecycle
************************************************************/

void clearSurfaceMap(
	SurfaceMap& surfaceMap)
{
	surfaceMap = {};
}

void rebuildSurfaceMap(
	SurfaceMap& surfaceMap,
	std::vector<DiscoveredSurfaceChunk>&& discoveredChunks)
{
	clearSurfaceMap(surfaceMap);

	std::sort(
		discoveredChunks.begin(),
		discoveredChunks.end(),
		isDiscoveredSurfaceChunkBefore);

	surfaceMap.chunks =
		std::move(discoveredChunks);

	surfaceMap.columns.reserve(
		surfaceMap.chunks.size());

	for (uint32_t chunkIndex = 0;
		chunkIndex < surfaceMap.chunks.size();
		++chunkIndex)
	{
		const DiscoveredSurfaceChunk& chunk =
			surfaceMap.chunks[chunkIndex];

		assert(!chunk.voxels.empty());

		const bool beginsNewColumn =
			surfaceMap.columns.empty() ||
			chunk.coord.x !=
			surfaceMap.columns.back().chunkX ||
			chunk.coord.z !=
			surfaceMap.columns.back().chunkZ;

		if (beginsNewColumn)
		{
			SurfaceChunkColumn column = {};

			column.chunkX = chunk.coord.x;
			column.chunkZ = chunk.coord.z;

			column.firstSurfaceChunkIndex =
				chunkIndex;

			surfaceMap.columns.push_back(
				column);
		}

		++surfaceMap.columns.back()
			.surfaceChunkCount;
	}
}
