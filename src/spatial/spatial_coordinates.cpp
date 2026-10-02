///////////////////////////////////////////////////////////////////////////////
// spatial/spatial_coordinates.cpp
// ===============================
//
// Implements coordinate conversion, normalization, indexing, and relative-
// position helpers for the project's spatial grid.
//
///////////////////////////////////////////////////////////////////////////////

#include "spatial/spatial_coordinates.h"

#include "spatial/spatial_constants.h"

#include <glm/common.hpp>

#include <cassert>
#include <cmath>
#include <limits>

/***********************************************************
* File-Local Helpers
************************************************************/

static void normalizeWorldPositionAxis(
	int32_t& chunkCoordinate,
	float& localPosition)
{
	const int64_t chunkOffset =
		static_cast<int64_t>(
			std::floor(
				localPosition /
				CHUNK_SIZE_METERS));

	const int64_t normalizedChunkCoordinate =
		static_cast<int64_t>(
			chunkCoordinate) +
		chunkOffset;

	assert(
		normalizedChunkCoordinate >=
		std::numeric_limits<int32_t>::min());

	assert(
		normalizedChunkCoordinate <=
		std::numeric_limits<int32_t>::max());

	chunkCoordinate =
		static_cast<int32_t>(
			normalizedChunkCoordinate);

	localPosition -=
		static_cast<float>(
			chunkOffset) *
		CHUNK_SIZE_METERS;
}

/***********************************************************
* Metric Coordinate Helpers
************************************************************/

WorldMetricCoordinate getWorldMetricCoordinate(
	int32_t chunkCoordinate,
	float localPosition)
{
	assert(localPosition >= 0.0f);
	assert(localPosition < CHUNK_SIZE_METERS);

	const int64_t localWholeMeters =
		static_cast<int64_t>(
			std::floor(
				localPosition));

	WorldMetricCoordinate axis = {};

	axis.wholeMeters =
		static_cast<int64_t>(
			chunkCoordinate) *
		static_cast<int64_t>(
			CHUNK_SIZE) +
		localWholeMeters;

	axis.fractionalMeter =
		localPosition -
		static_cast<float>(
			localWholeMeters);

	assert(axis.fractionalMeter >= 0.0f);
	assert(axis.fractionalMeter < 1.0f);

	return axis;
}

WorldMetricCoordinate offsetWorldMetricCoordinate(
	const WorldMetricCoordinate& axis,
	float offsetMeters)
{
	assert(axis.fractionalMeter >= 0.0f);
	assert(axis.fractionalMeter < 1.0f);

	const float totalFractionalMeters =
		axis.fractionalMeter +
		offsetMeters;

	const int64_t wholeMeterOffset =
		static_cast<int64_t>(
			std::floor(
				totalFractionalMeters));

	const int64_t resultWholeMeters =
		axis.wholeMeters +
		wholeMeterOffset;

	WorldMetricCoordinate result = {};

	result.wholeMeters =
		resultWholeMeters;

	result.fractionalMeter =
		totalFractionalMeters -
		static_cast<float>(
			wholeMeterOffset);

	assert(result.fractionalMeter >= 0.0f);
	assert(result.fractionalMeter < 1.0f);

	return result;
}

float getWorldMetricCoordinateOffset(
	const WorldMetricCoordinate& origin,
	const WorldMetricCoordinate& position)
{
	assert(origin.fractionalMeter >= 0.0f);
	assert(origin.fractionalMeter < 1.0f);

	assert(position.fractionalMeter >= 0.0f);
	assert(position.fractionalMeter < 1.0f);

	const int64_t wholeMeterDelta =
		position.wholeMeters -
		origin.wholeMeters;

	return
		static_cast<float>(
			wholeMeterDelta) +
		position.fractionalMeter -
		origin.fractionalMeter;
}

int32_t compareWorldMetricCoordinates(
	const WorldMetricCoordinate& a,
	const WorldMetricCoordinate& b)
{
	if (a.wholeMeters < b.wholeMeters)
	{
		return -1;
	}

	if (a.wholeMeters > b.wholeMeters)
	{
		return 1;
	}

	if (a.fractionalMeter < b.fractionalMeter)
	{
		return -1;
	}

	if (a.fractionalMeter > b.fractionalMeter)
	{
		return 1;
	}

	return 0;
}

/***********************************************************
* Coordinate Validation
************************************************************/

bool isWorldPositionCanonical(
	const WorldPosition& worldPosition)
{
	return
		worldPosition.localPosition.x >= 0.0f &&
		worldPosition.localPosition.y >= 0.0f &&
		worldPosition.localPosition.z >= 0.0f &&

		worldPosition.localPosition.x <
		CHUNK_SIZE_METERS &&
		worldPosition.localPosition.y <
		CHUNK_SIZE_METERS &&
		worldPosition.localPosition.z <
		CHUNK_SIZE_METERS;
}

bool isVoxelCoordInChunkBounds(
	const VoxelCoord& voxelCoord)
{
	return
		voxelCoord.x < CHUNK_SIZE &&
		voxelCoord.y < CHUNK_SIZE &&
		voxelCoord.z < CHUNK_SIZE;
}

bool isSampleCoordInChunkBounds(
	const SampleCoord& sampleCoord)
{
	return
		sampleCoord.x < CHUNK_SAMPLE_SIZE &&
		sampleCoord.y < CHUNK_SAMPLE_SIZE &&
		sampleCoord.z < CHUNK_SAMPLE_SIZE;
}

/***********************************************************
* World Position Helpers
************************************************************/

WorldPosition makeWorldPosition(
	const ChunkCoord& chunkCoord,
	const glm::vec3& localPosition)
{
	WorldPosition worldPosition = {};

	worldPosition.chunk =
		chunkCoord;

	worldPosition.localPosition =
		localPosition;

	normalizeWorldPositionAxis(
		worldPosition.chunk.x,
		worldPosition.localPosition.x);

	normalizeWorldPositionAxis(
		worldPosition.chunk.y,
		worldPosition.localPosition.y);

	normalizeWorldPositionAxis(
		worldPosition.chunk.z,
		worldPosition.localPosition.z);

	assert(
		isWorldPositionCanonical(
			worldPosition));

	return worldPosition;
}

glm::vec3 getChunkRelativePosition(
	const ChunkCoord& chunkCoord,
	const ChunkCoord& originChunk)
{
	const int64_t chunkDeltaX =
		static_cast<int64_t>(
			chunkCoord.x) -
		static_cast<int64_t>(
			originChunk.x);

	const int64_t chunkDeltaY =
		static_cast<int64_t>(
			chunkCoord.y) -
		static_cast<int64_t>(
			originChunk.y);

	const int64_t chunkDeltaZ =
		static_cast<int64_t>(
			chunkCoord.z) -
		static_cast<int64_t>(
			originChunk.z);

	return glm::vec3(
		static_cast<float>(
			chunkDeltaX) *
		CHUNK_SIZE_METERS,
		static_cast<float>(
			chunkDeltaY) *
		CHUNK_SIZE_METERS,
		static_cast<float>(
			chunkDeltaZ) *
		CHUNK_SIZE_METERS);
}

/***********************************************************
* Voxel Position Helpers
************************************************************/

glm::vec3 getVoxelLocalMin(
	const VoxelCoord& voxelCoord)
{
	assert(
		isVoxelCoordInChunkBounds(
			voxelCoord));

	return glm::vec3(
		static_cast<float>(
			voxelCoord.x) *
		VOXEL_SIZE_METERS,
		static_cast<float>(
			voxelCoord.y) *
		VOXEL_SIZE_METERS,
		static_cast<float>(
			voxelCoord.z) *
		VOXEL_SIZE_METERS);
}

glm::vec3 getVoxelLocalCenter(
	const VoxelCoord& voxelCoord)
{
	assert(
		isVoxelCoordInChunkBounds(
			voxelCoord));

	return glm::vec3(
		VOXEL_SIZE_METERS *
		(static_cast<float>(
			voxelCoord.x) +
			0.5f),
		VOXEL_SIZE_METERS *
		(static_cast<float>(
			voxelCoord.y) +
			0.5f),
		VOXEL_SIZE_METERS *
		(static_cast<float>(
			voxelCoord.z) +
			0.5f));
}

/***********************************************************
* Voxel Coordinate Conversion Helpers
************************************************************/

VoxelCoord getVoxelCoordFromChunkLocalPosition(
	const glm::vec3& localPosition)
{
	assert(localPosition.x >= 0.0f);
	assert(localPosition.y >= 0.0f);
	assert(localPosition.z >= 0.0f);

	assert(localPosition.x < CHUNK_SIZE_METERS);
	assert(localPosition.y < CHUNK_SIZE_METERS);
	assert(localPosition.z < CHUNK_SIZE_METERS);

	return
	{
		static_cast<uint32_t>(
			glm::floor(
				localPosition.x /
				VOXEL_SIZE_METERS)),

		static_cast<uint32_t>(
			glm::floor(
				localPosition.y /
				VOXEL_SIZE_METERS)),

		static_cast<uint32_t>(
			glm::floor(
				localPosition.z /
				VOXEL_SIZE_METERS))
	};
}

VoxelCoord getVoxelCoordFromWorldPosition(
	const WorldPosition& worldPosition)
{
	assert(
		isWorldPositionCanonical(
			worldPosition));

	return getVoxelCoordFromChunkLocalPosition(
		worldPosition.localPosition);
}

/***********************************************************
* Voxel Index Helpers
************************************************************/

uint32_t getVoxelIndexInChunk(
	const VoxelCoord& voxelCoord)
{
	assert(
		isVoxelCoordInChunkBounds(
			voxelCoord));

	return
		voxelCoord.x +
		voxelCoord.y *
		CHUNK_SIZE +
		voxelCoord.z *
		CHUNK_SIZE *
		CHUNK_SIZE;
}

VoxelCoord getVoxelCoordFromIndex(
	uint32_t voxelIndex)
{
	assert(
		voxelIndex <
		CHUNK_VOXEL_COUNT);

	VoxelCoord voxelCoord = {};

	voxelCoord.x =
		voxelIndex %
		CHUNK_SIZE;

	const uint32_t yz =
		voxelIndex /
		CHUNK_SIZE;

	voxelCoord.y =
		yz %
		CHUNK_SIZE;

	voxelCoord.z =
		yz /
		CHUNK_SIZE;

	return voxelCoord;
}

/***********************************************************
* Sample Position Helpers
************************************************************/

glm::vec3 getSampleLocalPosition(
	const SampleCoord& sampleCoord)
{
	assert(
		isSampleCoordInChunkBounds(
			sampleCoord));

	return glm::vec3(
		static_cast<float>(
			sampleCoord.x) *
		VOXEL_SIZE_METERS,
		static_cast<float>(
			sampleCoord.y) *
		VOXEL_SIZE_METERS,
		static_cast<float>(
			sampleCoord.z) *
		VOXEL_SIZE_METERS);
}

WorldPosition getSampleWorldPosition(
	const ChunkCoord& chunkCoord,
	const SampleCoord& sampleCoord)
{
	return makeWorldPosition(
		chunkCoord,
		getSampleLocalPosition(
			sampleCoord));
}

/***********************************************************
* Sample Index Helpers
************************************************************/

uint32_t getSampleIndexInChunk(
	const SampleCoord& sampleCoord)
{
	assert(
		isSampleCoordInChunkBounds(
			sampleCoord));

	return
		sampleCoord.x +
		sampleCoord.y *
		CHUNK_SAMPLE_SIZE +
		sampleCoord.z *
		CHUNK_SAMPLE_SIZE *
		CHUNK_SAMPLE_SIZE;
}

SampleCoord getSampleCoordFromIndex(
	uint32_t sampleIndex)
{
	assert(
		sampleIndex <
		CHUNK_SAMPLE_COUNT);

	SampleCoord sampleCoord = {};

	sampleCoord.x =
		sampleIndex %
		CHUNK_SAMPLE_SIZE;

	const uint32_t yz =
		sampleIndex /
		CHUNK_SAMPLE_SIZE;

	sampleCoord.y =
		yz %
		CHUNK_SAMPLE_SIZE;

	sampleCoord.z =
		yz /
		CHUNK_SAMPLE_SIZE;

	return sampleCoord;
}
