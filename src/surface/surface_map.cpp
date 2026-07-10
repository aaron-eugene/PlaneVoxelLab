///////////////////////////////////////////////////////////////////////////////
// surface/surface_map.cpp
// =======================
//
// Implements surface-crossing lookup generation from sampled chunk density data.
//
///////////////////////////////////////////////////////////////////////////////

#include "surface/surface_map.h"

#include "chunk/chunk.h"
#include "geometry/voxel_topology.h"
#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"

#include <cassert>
#include <cstdint>
#include <vector>

/***********************************************************
* Constants
************************************************************/

static constexpr float SURFACE_MAP_DENSITY_EPSILON = 0.00001f;

/***********************************************************
* Local Helpers
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

static bool rebuildSurfaceChunk(
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

	return true;
}

/***********************************************************
* Surface Map Lifecycle
************************************************************/

void clearSurfaceMap(
	SurfaceMap& surfaceMap)
{
	surfaceMap = {};
}

/***********************************************************
* Surface Map Rebuild
************************************************************/

bool rebuildSurfaceMap(
	SurfaceMap& surfaceMap,
	const std::vector<Chunk>& chunks)
{
	clearSurfaceMap(surfaceMap);

	surfaceMap.chunks.reserve(chunks.size());

	for (uint32_t chunkIndex = 0;
		chunkIndex < chunks.size();
		++chunkIndex)
	{
		SurfaceChunk surfaceChunk = {};

		if (!rebuildSurfaceChunk(
			surfaceChunk,
			chunks[chunkIndex],
			chunkIndex))
		{
			clearSurfaceMap(surfaceMap);
			return false;
		}

		if (surfaceChunk.voxels.empty())
		{
			continue;
		}

		surfaceMap.chunks.push_back(surfaceChunk);
	}

	return true;
}
