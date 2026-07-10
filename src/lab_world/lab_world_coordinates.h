///////////////////////////////////////////////////////////////////////////////
// lab_world/lab_world_coordinates.h
// =================================
//
// Declares coordinate types and inline conversion helpers for the lab world's
// chunk, voxel, sample, local-position, and world-position systems.
//
// These helpers provide the canonical way to translate positions and indices
// across the lab world's spatial domains.
//
// No functions in this file depend on the Chunk struct. Everything operates on
// primitive coordinate types and GLM vectors only.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "lab_world/lab_world_constants.h"

#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/common.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>

/***********************************************************
* Coordinate Types
************************************************************/

struct ChunkCoord
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
};

struct VoxelCoord
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;
};

struct SampleCoord
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;
};

/***********************************************************
* Coordinate Validation
************************************************************/

inline bool isVoxelCoordInChunkBounds(
	const VoxelCoord& voxelCoord)
{
	return
		voxelCoord.x < CHUNK_SIZE &&
		voxelCoord.y < CHUNK_SIZE &&
		voxelCoord.z < CHUNK_SIZE;
}

inline bool isSampleCoordInChunkBounds(
	const SampleCoord& sampleCoord)
{
	return
		sampleCoord.x < CHUNK_SAMPLE_SIZE &&
		sampleCoord.y < CHUNK_SAMPLE_SIZE &&
		sampleCoord.z < CHUNK_SAMPLE_SIZE;
}

/***********************************************************
* Chunk Coordinate Helpers
************************************************************/

inline ChunkCoord getChunkCoordFromWorldPosition(
	const glm::dvec3& worldPosition)
{
	return {
		static_cast<int32_t>(glm::floor(worldPosition.x / CHUNK_SIZE_METERS_D)),
		static_cast<int32_t>(glm::floor(worldPosition.y / CHUNK_SIZE_METERS_D)),
		static_cast<int32_t>(glm::floor(worldPosition.z / CHUNK_SIZE_METERS_D))
	};
}

inline glm::dvec3 getChunkWorldMin(
	const ChunkCoord& chunkCoord)
{
	return glm::dvec3(
		static_cast<double>(chunkCoord.x) * CHUNK_SIZE_METERS_D,
		static_cast<double>(chunkCoord.y) * CHUNK_SIZE_METERS_D,
		static_cast<double>(chunkCoord.z) * CHUNK_SIZE_METERS_D);
}

/***********************************************************
* World <-> Chunk-Local Position Helpers
************************************************************/

inline glm::dvec3 getWorldPositionFromChunkLocalPosition(
	const ChunkCoord& chunkCoord,
	const glm::vec3& localPosition)
{
	return getChunkWorldMin(chunkCoord) + glm::dvec3(localPosition);
}

inline glm::vec3 getChunkLocalPositionFromWorldPosition(
	const ChunkCoord& chunkCoord,
	const glm::dvec3& worldPosition)
{
	const glm::dvec3 localPosition =
		worldPosition - getChunkWorldMin(chunkCoord);

	return glm::vec3(localPosition);
}

/***********************************************************
* Voxel Position Helpers
************************************************************/

inline glm::vec3 getVoxelLocalMin(
	const VoxelCoord& voxelCoord)
{
	assert(isVoxelCoordInChunkBounds(voxelCoord));

	return glm::vec3(
		static_cast<float>(voxelCoord.x) * VOXEL_SIZE_METERS,
		static_cast<float>(voxelCoord.y) * VOXEL_SIZE_METERS,
		static_cast<float>(voxelCoord.z) * VOXEL_SIZE_METERS);
}

inline glm::vec3 getVoxelLocalCenter(
	const VoxelCoord& voxelCoord)
{
	assert(isVoxelCoordInChunkBounds(voxelCoord));

	return glm::vec3(
		VOXEL_SIZE_METERS * (static_cast<float>(voxelCoord.x) + 0.5f),
		VOXEL_SIZE_METERS * (static_cast<float>(voxelCoord.y) + 0.5f),
		VOXEL_SIZE_METERS * (static_cast<float>(voxelCoord.z) + 0.5f));
}

/***********************************************************
* Voxel Coordinate Conversion Helpers
************************************************************/

inline VoxelCoord getVoxelCoordFromChunkLocalPosition(
	const glm::vec3& localPosition)
{
	VoxelCoord voxelCoord = {
		static_cast<uint32_t>(
			glm::floor(localPosition.x / VOXEL_SIZE_METERS)),
		static_cast<uint32_t>(
			glm::floor(localPosition.y / VOXEL_SIZE_METERS)),
		static_cast<uint32_t>(
			glm::floor(localPosition.z / VOXEL_SIZE_METERS))
	};

	assert(isVoxelCoordInChunkBounds(voxelCoord));

	return voxelCoord;
}

inline VoxelCoord getVoxelCoordFromWorldPosition(
	const ChunkCoord& chunkCoord,
	const glm::dvec3& worldPosition)
{
	const glm::vec3 localPosition =
		getChunkLocalPositionFromWorldPosition(
			chunkCoord,
			worldPosition);

	return getVoxelCoordFromChunkLocalPosition(localPosition);
}

inline void getChunkAndVoxelCoordFromWorldPosition(
	const glm::dvec3& worldPosition,
	ChunkCoord& outChunkCoord,
	VoxelCoord& outVoxelCoord)
{
	outChunkCoord = getChunkCoordFromWorldPosition(worldPosition);

	outVoxelCoord = getVoxelCoordFromWorldPosition(
		outChunkCoord,
		worldPosition);
}

/***********************************************************
* Voxel Index Helpers
************************************************************/

inline uint32_t getVoxelIndexInChunk(
	const VoxelCoord& voxelCoord)
{
	assert(isVoxelCoordInChunkBounds(voxelCoord));

	return
		voxelCoord.x +
		voxelCoord.y * CHUNK_SIZE +
		voxelCoord.z * CHUNK_SIZE * CHUNK_SIZE;
}

inline VoxelCoord getVoxelCoordFromIndex(
	uint32_t voxelIndex)
{
	assert(voxelIndex < CHUNK_VOXEL_COUNT);

	VoxelCoord voxelCoord = {};
	voxelCoord.x = voxelIndex % CHUNK_SIZE;

	const uint32_t yz = voxelIndex / CHUNK_SIZE;
	voxelCoord.y = yz % CHUNK_SIZE;
	voxelCoord.z = yz / CHUNK_SIZE;

	return voxelCoord;
}

/***********************************************************
* Sample Position Helpers
************************************************************/

inline glm::vec3 getSampleLocalPosition(
	const SampleCoord& sampleCoord)
{
	assert(isSampleCoordInChunkBounds(sampleCoord));

	return glm::vec3(
		static_cast<float>(sampleCoord.x) * VOXEL_SIZE_METERS,
		static_cast<float>(sampleCoord.y) * VOXEL_SIZE_METERS,
		static_cast<float>(sampleCoord.z) * VOXEL_SIZE_METERS);
}

inline glm::dvec3 getSampleWorldPosition(
	const ChunkCoord& chunkCoord,
	const SampleCoord& sampleCoord)
{
	return getWorldPositionFromChunkLocalPosition(
		chunkCoord,
		getSampleLocalPosition(sampleCoord));
}

/***********************************************************
* Sample Index Helpers
************************************************************/

inline uint32_t getSampleIndexInChunk(
	const SampleCoord& sampleCoord)
{
	assert(isSampleCoordInChunkBounds(sampleCoord));

	return
		sampleCoord.x +
		sampleCoord.y * CHUNK_SAMPLE_SIZE +
		sampleCoord.z * CHUNK_SAMPLE_SIZE * CHUNK_SAMPLE_SIZE;
}

inline SampleCoord getSampleCoordFromIndex(
	uint32_t sampleIndex)
{
	assert(sampleIndex < CHUNK_SAMPLE_COUNT);

	SampleCoord sampleCoord = {};
	sampleCoord.x = sampleIndex % CHUNK_SAMPLE_SIZE;

	const uint32_t yz = sampleIndex / CHUNK_SAMPLE_SIZE;
	sampleCoord.y = yz % CHUNK_SAMPLE_SIZE;
	sampleCoord.z = yz / CHUNK_SAMPLE_SIZE;

	return sampleCoord;
}
