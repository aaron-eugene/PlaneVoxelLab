///////////////////////////////////////////////////////////////////////////////
// surface_discovery/surface_discovery.cpp
// =======================================
//
// Implements generic and heightmap-specific surface discovery.
//
///////////////////////////////////////////////////////////////////////////////

#include "surface_discovery/surface_discovery.h"

#include "chunk/chunk.h"
#include "chunk/chunk_sampling.h"
#include "fields/density_field.h"
#include "fields/field_generators.h"
#include "geometry/voxel_topology.h"
#include "spatial/spatial_constants.h"
#include "spatial/spatial_coordinates.h"

#include <algorithm>
#include <cstdint>
#include <utility>

/***********************************************************
* Surface Crossing Helpers
************************************************************/

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
	bool hasInside = false;
	bool hasOutside = false;

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

		if (isDensityInside(density))
		{
			hasInside = true;
		}
		else
		{
			hasOutside = true;
		}

		if (hasInside && hasOutside)
		{
			return true;
		}
	}

	return false;
}

/***********************************************************
* Surface Chunk Discovery Helpers
************************************************************/

static bool isHeightmapSurfaceChunkCandidate(
	const ChunkHeightRange& range,
	int32_t chunkY)
{
	const WorldMetricCoordinate chunkBottom =
		getWorldMetricCoordinate(
			chunkY,
			0.0f);

	const WorldMetricCoordinate chunkTop =
		getWorldMetricCoordinate(
			chunkY + 1,
			0.0f);

	// Inclusive bounds preserve candidates at chunk boundaries.

	if (compareWorldMetricCoordinates(
		range.maxHeight,
		chunkBottom) < 0)
	{
		return false;
	}

	if (compareWorldMetricCoordinates(
		range.minHeight,
		chunkTop) > 0)
	{
		return false;
	}

	return true;
}

static void appendDiscoveredSurfaceChunk(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const Chunk& chunk,
	uint32_t chunkIndex)
{
	DiscoveredSurfaceChunk discoveredChunk = {};

	discoveredChunk.chunkIndex = chunkIndex;
	discoveredChunk.coord = chunk.coord;

	discoverChunkSurfaceVoxels(
		discoveredChunk.voxels,
		chunk);

	if (discoveredChunk.voxels.empty())
	{
		return;
	}

	discoveredChunks.push_back(
		std::move(discoveredChunk));
}

static bool isChunkIndexBeforeInColumnOrder(
	const std::vector<Chunk>& chunks,
	uint32_t indexA,
	uint32_t indexB)
{
	const ChunkCoord& coordA =
		chunks[indexA].coord;

	const ChunkCoord& coordB =
		chunks[indexB].coord;

	if (coordA.x != coordB.x)
	{
		return coordA.x < coordB.x;
	}

	if (coordA.z != coordB.z)
	{
		return coordA.z < coordB.z;
	}

	if (coordA.y != coordB.y)
	{
		return coordA.y < coordB.y;
	}

	return indexA < indexB;
}

/***********************************************************
* Surface Discovery
************************************************************/

void discoverChunkSurfaceVoxels(
	std::vector<SurfaceVoxel>& surfaceVoxels,
	const Chunk& chunk)
{
	surfaceVoxels.clear();

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

		surfaceVoxels.push_back(
			surfaceVoxel);
	}
}

void discoverSurfaceChunks(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const std::vector<Chunk>& chunks)
{
	discoveredChunks.clear();

	for (uint32_t chunkIndex = 0;
		chunkIndex < chunks.size();
		++chunkIndex)
	{
		appendDiscoveredSurfaceChunk(
			discoveredChunks,
			chunks[chunkIndex],
			chunkIndex);
	}
}

void discoverHeightmapSurfaceChunks(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const std::vector<Chunk>& chunks,
	const HeightmapDensityField& heightmap)
{
	discoveredChunks.clear();

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
			uint32_t indexA,
			uint32_t indexB)
		{
			return isChunkIndexBeforeInColumnOrder(
				chunks,
				indexA,
				indexB);
		});

	ChunkHeightGrid heightGrid = {};
	ChunkHeightRange heightRange = {};

	ChunkCoord currentColumnCoord = {};
	bool hasCurrentColumn = false;

	for (uint32_t chunkIndex :
	orderedChunkIndices)
	{
		const Chunk& chunk =
			chunks[chunkIndex];

		const bool beginsNewColumn =
			!hasCurrentColumn ||
			chunk.coord.x != currentColumnCoord.x ||
			chunk.coord.z != currentColumnCoord.z;

		if (beginsNewColumn)
		{
			currentColumnCoord =
			{
				chunk.coord.x,
				0,
				chunk.coord.z
			};

			sampleChunkHeightGrid(
				heightGrid,
				currentColumnCoord,
				heightmap);

			heightRange =
				getChunkHeightRange(heightGrid);

			hasCurrentColumn = true;
		}

		if (!isHeightmapSurfaceChunkCandidate(
			heightRange,
			chunk.coord.y))
		{
			continue;
		}

		appendDiscoveredSurfaceChunk(
			discoveredChunks,
			chunk,
			chunkIndex);
	}
}

