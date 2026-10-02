///////////////////////////////////////////////////////////////////////////////
// spatial/spatial_coordinates.h
// =============================
//
// Declares coordinate types and conversion helpers for chunk, voxel, sample,
// chunk-local, and world-space coordinate systems.
//
// These helpers provide the canonical conversions and indexing rules used by
// the project's spatial grid.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_constants.h"

#include <glm/ext/vector_float3.hpp>

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

struct WorldPosition
{
	ChunkCoord chunk = {};
	glm::vec3 localPosition = {};
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

struct WorldMetricCoordinate
{
	int64_t wholeMeters = 0;
	float fractionalMeter = 0.0f;
};

/***********************************************************
* Metric Coordinate Helpers
************************************************************/

WorldMetricCoordinate getWorldMetricCoordinate(
	int32_t chunkCoordinate,
	float localPosition);

WorldMetricCoordinate offsetWorldMetricCoordinate(
	const WorldMetricCoordinate& axis,
	float offsetMeters);

float getWorldMetricCoordinateOffset(
	const WorldMetricCoordinate& origin,
	const WorldMetricCoordinate& position);

int32_t compareWorldMetricCoordinates(
	const WorldMetricCoordinate& a,
	const WorldMetricCoordinate& b);

/***********************************************************
* Coordinate Validation
************************************************************/

bool isWorldPositionCanonical(
	const WorldPosition& worldPosition);

bool isVoxelCoordInChunkBounds(
	const VoxelCoord& voxelCoord);

bool isSampleCoordInChunkBounds(
	const SampleCoord& sampleCoord);

/***********************************************************
* World Position Helpers
************************************************************/

WorldPosition makeWorldPosition(
	const ChunkCoord& chunkCoord,
	const glm::vec3& localPosition);

glm::vec3 getChunkRelativePosition(
	const ChunkCoord& chunkCoord,
	const ChunkCoord& originChunk);

/***********************************************************
* Voxel Position Helpers
************************************************************/

glm::vec3 getVoxelLocalMin(
	const VoxelCoord& voxelCoord);

glm::vec3 getVoxelLocalCenter(
	const VoxelCoord& voxelCoord);

/***********************************************************
* Voxel Coordinate Conversion Helpers
************************************************************/

VoxelCoord getVoxelCoordFromChunkLocalPosition(
	const glm::vec3& localPosition);

VoxelCoord getVoxelCoordFromWorldPosition(
	const WorldPosition& worldPosition);

/***********************************************************
* Voxel Index Helpers
************************************************************/

uint32_t getVoxelIndexInChunk(
	const VoxelCoord& voxelCoord);

VoxelCoord getVoxelCoordFromIndex(
	uint32_t voxelIndex);

/***********************************************************
* Sample Position Helpers
************************************************************/

glm::vec3 getSampleLocalPosition(
	const SampleCoord& sampleCoord);

WorldPosition getSampleWorldPosition(
	const ChunkCoord& chunkCoord,
	const SampleCoord& sampleCoord);

/***********************************************************
* Sample Index Helpers
************************************************************/

uint32_t getSampleIndexInChunk(
	const SampleCoord& sampleCoord);

SampleCoord getSampleCoordFromIndex(
	uint32_t sampleIndex);