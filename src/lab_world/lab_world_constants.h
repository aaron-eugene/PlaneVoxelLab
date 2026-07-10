///////////////////////////////////////////////////////////////////////////////
// lab_world/lab_world_constants.h
// ===============================
//
// Defines shared constants for the lab world's voxel grid, chunk layout, and
// world-space unit scale.
//
// These constants describe the common data model consumed by chunks, geometry,
// surface references, and active terrain experiments.
//
// By convention, chunks are cube-shaped and have the same voxel count along
// each axis.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Unit Constants
************************************************************/

static constexpr float METERS_PER_UNIT = 1.0f;

/***********************************************************
* Voxel Constants
************************************************************/

static constexpr float VOXEL_SIZE_METERS =
	1.0f * METERS_PER_UNIT;

static constexpr uint32_t VOXEL_CORNER_COUNT = 8;
static constexpr uint32_t VOXEL_EDGE_COUNT = 12;
static constexpr uint32_t VOXEL_FACE_COUNT = 6;

/***********************************************************
* Chunk Constants
************************************************************/

static constexpr uint32_t CHUNK_SIZE = 16;

static constexpr float CHUNK_SIZE_METERS_F =
	VOXEL_SIZE_METERS * static_cast<float>(CHUNK_SIZE);

static constexpr double CHUNK_SIZE_METERS_D =
	static_cast<double>(VOXEL_SIZE_METERS) *
	static_cast<double>(CHUNK_SIZE);

static constexpr uint32_t CHUNK_VOXEL_COUNT =
	CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

static constexpr uint32_t CHUNK_SAMPLE_SIZE =
	CHUNK_SIZE + 1;

static constexpr uint32_t CHUNK_SAMPLE_COUNT =
	CHUNK_SAMPLE_SIZE * CHUNK_SAMPLE_SIZE * CHUNK_SAMPLE_SIZE;

/***********************************************************
* Lab World Limits
************************************************************/

//static constexpr uint32_t MAX_LAB_CHUNKS = 1;
constexpr int CHUNK_LOAD_RADIUS_X = 3;
constexpr int CHUNK_LOAD_RADIUS_Y = 2;
constexpr int CHUNK_LOAD_RADIUS_Z = 3;

static constexpr uint32_t CHUNK_LOAD_COUNT_X =
static_cast<uint32_t>(CHUNK_LOAD_RADIUS_X * 2);

static constexpr uint32_t CHUNK_LOAD_COUNT_Y =
static_cast<uint32_t>(CHUNK_LOAD_RADIUS_Y * 2);

static constexpr uint32_t CHUNK_LOAD_COUNT_Z =
static_cast<uint32_t>(CHUNK_LOAD_RADIUS_Z * 2);

static constexpr uint32_t CHUNK_LOAD_COUNT =
CHUNK_LOAD_COUNT_X *
CHUNK_LOAD_COUNT_Y *
CHUNK_LOAD_COUNT_Z;
