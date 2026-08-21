///////////////////////////////////////////////////////////////////////////////
// spatial/spatial_constants.h
// ===========================
//
// Defines the shared voxel-grid, chunk-grid, and world-unit constants used by
// Plane Voxel Lab's spatial systems.
//
// Chunks are cube-shaped and use the same voxel count along each axis.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Unit Constants
************************************************************/

constexpr float METERS_PER_UNIT = 1.0f;

/***********************************************************
* Voxel Constants
************************************************************/

constexpr float VOXEL_SIZE_METERS =
1.0f * METERS_PER_UNIT;

/***********************************************************
* Chunk Constants
************************************************************/

constexpr uint32_t CHUNK_SIZE = 16;

constexpr float CHUNK_SIZE_METERS_F =
VOXEL_SIZE_METERS * static_cast<float>(CHUNK_SIZE);

constexpr double CHUNK_SIZE_METERS_D =
static_cast<double>(VOXEL_SIZE_METERS) *
static_cast<double>(CHUNK_SIZE);

constexpr uint32_t CHUNK_VOXEL_COUNT =
CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

constexpr uint32_t CHUNK_SAMPLE_SIZE =
CHUNK_SIZE + 1;

constexpr uint32_t CHUNK_SAMPLE_COUNT =
CHUNK_SAMPLE_SIZE *
CHUNK_SAMPLE_SIZE *
CHUNK_SAMPLE_SIZE;
