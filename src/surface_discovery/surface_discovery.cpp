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
#include <vector>

// TEST
#include <cstdio>

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
	uint32_t chunkIndex,
	std::vector<SurfaceVoxel>&& surfaceVoxels)
{
	if (surfaceVoxels.empty())
	{
		return;
	}

	DiscoveredSurfaceChunk discoveredChunk = {};

	discoveredChunk.chunkIndex = chunkIndex;
	discoveredChunk.coord = chunk.coord;
	discoveredChunk.voxels = std::move(surfaceVoxels);

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
* Heightmap Voxel Candidate Helpers
************************************************************/

static ChunkHeightRange getHeightmapCellHeightRange(
	const ChunkHeightGrid& heightGrid,
	uint32_t voxelX,
	uint32_t voxelZ)
{
	ChunkHeightRange range = {};

	const uint32_t firstIndex =
		voxelX +
		voxelZ * CHUNK_SAMPLE_SIZE;

	const uint32_t cornerIndices[4] =
	{
		firstIndex,
		firstIndex + 1,
		firstIndex + CHUNK_SAMPLE_SIZE,
		firstIndex + CHUNK_SAMPLE_SIZE + 1
	};

	range.minHeight =
		heightGrid.heights[cornerIndices[0]];

	range.maxHeight =
		range.minHeight;

	for (uint32_t cornerIndex = 1;
		cornerIndex < 4;
		++cornerIndex)
	{
		const WorldMetricCoordinate& height =
			heightGrid.heights[cornerIndices[cornerIndex]];

		if (compareWorldMetricCoordinates(
			height,
			range.minHeight) < 0)
		{
			range.minHeight = height;
		}

		if (compareWorldMetricCoordinates(
			height,
			range.maxHeight) > 0)
		{
			range.maxHeight = height;
		}
	}

	return range;
}

static WorldMetricCoordinate getChunkVoxelYBoundary(
	const ChunkCoord& chunkCoord,
	uint32_t sampleY)
{
	const SampleCoord sampleCoord =
	{
		0,
		sampleY,
		0
	};

	const WorldPosition worldPosition =
		getSampleWorldPosition(
			chunkCoord,
			sampleCoord);

	return getWorldMetricCoordinate(
		worldPosition.chunk.y,
		worldPosition.localPosition.y);
}

static bool getHeightmapCandidateVoxelYRange(
	uint32_t& firstVoxelY,
	uint32_t& endVoxelY,
	const ChunkHeightRange& range,
	const ChunkCoord& chunkCoord)
{
	//--------------------------------------------------
	// Find the first voxel whose top reaches minHeight.
	//--------------------------------------------------

	uint32_t low = 0;
	uint32_t high = CHUNK_SIZE;

	while (low < high)
	{
		const uint32_t middle =
			low + (high - low) / 2;

		const WorldMetricCoordinate voxelTop =
			getChunkVoxelYBoundary(
				chunkCoord,
				middle + 1);

		if (compareWorldMetricCoordinates(
			voxelTop,
			range.minHeight) < 0)
		{
			low = middle + 1;
		}
		else
		{
			high = middle;
		}
	}

	firstVoxelY = low;

	//--------------------------------------------------
	// Find the first voxel whose bottom exceeds maxHeight.
	//--------------------------------------------------

	low = 0;
	high = CHUNK_SIZE;

	while (low < high)
	{
		const uint32_t middle =
			low + (high - low) / 2;

		const WorldMetricCoordinate voxelBottom =
			getChunkVoxelYBoundary(
				chunkCoord,
				middle);

		if (compareWorldMetricCoordinates(
			voxelBottom,
			range.maxHeight) <= 0)
		{
			low = middle + 1;
		}
		else
		{
			high = middle;
		}
	}

	endVoxelY = low;

	return firstVoxelY < endVoxelY;
}

// TEMP
static bool isHeightmapSurfaceVoxelCandidate(
	const ChunkHeightRange& range,
	const ChunkCoord& chunkCoord,
	uint32_t voxelY)
{
	const SampleCoord bottomSample =
	{
		0,
		voxelY,
		0
	};

	const SampleCoord topSample =
	{
		0,
		voxelY + 1,
		0
	};

	const WorldPosition bottomPosition =
		getSampleWorldPosition(
			chunkCoord,
			bottomSample);

	const WorldPosition topPosition =
		getSampleWorldPosition(
			chunkCoord,
			topSample);

	const WorldMetricCoordinate voxelBottom =
		getWorldMetricCoordinate(
			bottomPosition.chunk.y,
			bottomPosition.localPosition.y);

	const WorldMetricCoordinate voxelTop =
		getWorldMetricCoordinate(
			topPosition.chunk.y,
			topPosition.localPosition.y);

	// Inclusive bounds preserve potential crossings
	// at exact voxel boundaries.

	if (compareWorldMetricCoordinates(
		range.maxHeight,
		voxelBottom) < 0)
	{
		return false;
	}

	if (compareWorldMetricCoordinates(
		range.minHeight,
		voxelTop) > 0)
	{
		return false;
	}

	return true;
}

/***********************************************************
* Voxel Surface Discovery
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

void discoverHeightmapChunkSurfaceVoxels(
	std::vector<SurfaceVoxel>& surfaceVoxels,
	const Chunk& chunk,
	const ChunkHeightGrid& heightGrid)
{
	surfaceVoxels.clear();

	for (uint32_t voxelZ = 0;
		voxelZ < CHUNK_SIZE;
		++voxelZ)
	{
		for (uint32_t voxelX = 0;
			voxelX < CHUNK_SIZE;
			++voxelX)
		{
			const ChunkHeightRange cellRange =
				getHeightmapCellHeightRange(
					heightGrid,
					voxelX,
					voxelZ);

			uint32_t firstVoxelY = 0;
			uint32_t endVoxelY = 0;

			if (!getHeightmapCandidateVoxelYRange(
				firstVoxelY,
				endVoxelY,
				cellRange,
				chunk.coord))
			{
				continue;
			}

			for (uint32_t voxelY = firstVoxelY;
				voxelY < endVoxelY;
				++voxelY)
			{
				const VoxelCoord voxelCoord =
				{
					voxelX,
					voxelY,
					voxelZ
				};

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
	}
}

/***********************************************************
* Chunk Surface Discovery
************************************************************/

void discoverSurfaceChunks(
	std::vector<DiscoveredSurfaceChunk>& discoveredChunks,
	const std::vector<Chunk>& chunks)
{
	discoveredChunks.clear();

	for (uint32_t chunkIndex = 0;
		chunkIndex < chunks.size();
		++chunkIndex)
	{
		std::vector<SurfaceVoxel> surfaceVoxels = {};

		discoverChunkSurfaceVoxels(
			surfaceVoxels,
			chunks[chunkIndex]);

		appendDiscoveredSurfaceChunk(
			discoveredChunks,
			chunks[chunkIndex],
			chunkIndex,
			std::move(surfaceVoxels));
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
				getChunkHeightRange(
					heightGrid);

			hasCurrentColumn = true;
		}

		if (!isHeightmapSurfaceChunkCandidate(
			heightRange,
			chunk.coord.y))
		{
			continue;
		}

		std::vector<SurfaceVoxel> surfaceVoxels = {};

		discoverHeightmapChunkSurfaceVoxels(
			surfaceVoxels,
			chunk,
			heightGrid);

		appendDiscoveredSurfaceChunk(
			discoveredChunks,
			chunk,
			chunkIndex,
			std::move(surfaceVoxels));
	}
}
