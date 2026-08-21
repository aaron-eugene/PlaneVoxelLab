///////////////////////////////////////////////////////////////////////////////
// surface_map/surface_map.cpp
// ===========================
//
// Implements surface-crossing lookup generation from sampled chunk density data.
//
///////////////////////////////////////////////////////////////////////////////

#include "surface_map/surface_map.h"

#include "chunk/chunk.h"
#include "geometry/voxel_topology.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

/***********************************************************
* Constants
************************************************************/

static constexpr float SURFACE_MAP_DENSITY_EPSILON = 0.00001f;

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

static bool isChunkIndexBeforeInSurfaceMapOrder(
	const std::vector<Chunk>& chunks,
	uint32_t chunkIndexA,
	uint32_t chunkIndexB)
{
	assert(chunkIndexA < chunks.size());
	assert(chunkIndexB < chunks.size());

	const ChunkCoord& coordA =
		chunks[chunkIndexA].coord;

	const ChunkCoord& coordB =
		chunks[chunkIndexB].coord;

	if (isChunkCoordBeforeInSurfaceMapOrder(
		coordA,
		coordB))
	{
		return true;
	}

	if (isChunkCoordBeforeInSurfaceMapOrder(
		coordB,
		coordA))
	{
		return false;
	}

	return chunkIndexA < chunkIndexB;
}

static bool isSurfaceChunkInColumn(
	const SurfaceChunk& surfaceChunk,
	const SurfaceChunkColumn& column)
{
	return
		surfaceChunk.coord.x ==
		column.chunkX &&
		surfaceChunk.coord.z ==
		column.chunkZ;
}

static SampleCoord getVoxelCornerSampleCoord(
	const VoxelCoord& voxelCoord,
	uint32_t cornerIndex)
{
	const VoxelCornerGridOffset cornerOffset =
		getVoxelCornerGridOffset(cornerIndex);

	const SampleCoord sampleCoord =
	{
		voxelCoord.x + cornerOffset.x,
		voxelCoord.y + cornerOffset.y,
		voxelCoord.z + cornerOffset.z
	};

	return sampleCoord;
}

static bool doesVoxelContainSurfaceCrossing(
	const Chunk& chunk,
	const VoxelCoord& voxelCoord)
{
	bool hasNegative = false;
	bool hasPositive = false;
	bool hasZero = false;

	for (uint32_t cornerIndex = 0;
		cornerIndex < VOXEL_CORNER_COUNT;
		++cornerIndex)
	{
		const SampleCoord sampleCoord =
			getVoxelCornerSampleCoord(
				voxelCoord,
				cornerIndex);

		const float density =
			getChunkDensitySample(
				chunk,
				sampleCoord);

		if (density < -SURFACE_MAP_DENSITY_EPSILON)
		{
			hasNegative = true;
		}
		else if (density > SURFACE_MAP_DENSITY_EPSILON)
		{
			hasPositive = true;
		}
		else
		{
			hasZero = true;
		}
	}

	return
		(hasNegative && hasPositive) ||
		(hasZero && (hasNegative || hasPositive));
}

static void rebuildSurfaceChunk(
	SurfaceChunk& surfaceChunk,
	const Chunk& chunk,
	uint32_t chunkIndex)
{
	surfaceChunk = {};
	surfaceChunk.chunkIndex = chunkIndex;
	surfaceChunk.coord = chunk.coord;

	for (uint32_t voxelIndex = 0;
		voxelIndex < CHUNK_VOXEL_COUNT;
		++voxelIndex)
	{
		const VoxelCoord voxelCoord =
			getVoxelCoordFromIndex(voxelIndex);

		if (!doesVoxelContainSurfaceCrossing(
			chunk,
			voxelCoord))
		{
			continue;
		}

		SurfaceVoxel surfaceVoxel = {};
		surfaceVoxel.coord = voxelCoord;

		surfaceChunk.voxels.push_back(surfaceVoxel);
	}
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
	const std::vector<Chunk>& chunks)
{
	clearSurfaceMap(surfaceMap);

	surfaceMap.chunks.reserve(
		chunks.size());

	surfaceMap.columns.reserve(
		chunks.size());

	std::vector<uint32_t> orderedChunkIndices = {};

	orderedChunkIndices.reserve(
		chunks.size());

	for (uint32_t chunkIndex = 0;
		chunkIndex < chunks.size();
		++chunkIndex)
	{
		orderedChunkIndices.push_back(
			chunkIndex);
	}

	std::sort(
		orderedChunkIndices.begin(),
		orderedChunkIndices.end(),
		[&chunks](
			uint32_t chunkIndexA,
			uint32_t chunkIndexB)
		{
			return isChunkIndexBeforeInSurfaceMapOrder(
				chunks,
				chunkIndexA,
				chunkIndexB);
		});

	for (uint32_t chunkIndex :
	orderedChunkIndices)
	{
		assert(chunkIndex < chunks.size());

		SurfaceChunk surfaceChunk = {};

		rebuildSurfaceChunk(
			surfaceChunk,
			chunks[chunkIndex],
			chunkIndex);

		if (surfaceChunk.voxels.empty())
		{
			continue;
		}

		const bool beginsNewColumn =
			surfaceMap.columns.empty() ||
			!isSurfaceChunkInColumn(
				surfaceChunk,
				surfaceMap.columns.back());

		if (beginsNewColumn)
		{
			SurfaceChunkColumn column = {};

			column.chunkX =
				surfaceChunk.coord.x;

			column.chunkZ =
				surfaceChunk.coord.z;

			column.firstSurfaceChunkIndex =
				static_cast<uint32_t>(
					surfaceMap.chunks.size());

			surfaceMap.columns.push_back(
				column);
		}

		surfaceMap.chunks.push_back(
			std::move(surfaceChunk));

		++surfaceMap.columns.back()
			.surfaceChunkCount;
	}
}
