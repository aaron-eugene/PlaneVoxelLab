///////////////////////////////////////////////////////////////////////////////
// lab_world/lab_world_constants.h
// ===============================
//
// Defines LabWorld-specific loading limits and chunk-count configuration.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Lab World Loading
************************************************************/

constexpr int CHUNK_LOAD_RADIUS_X = 4;
constexpr int CHUNK_LOAD_RADIUS_Y = 3;
constexpr int CHUNK_LOAD_RADIUS_Z = 4;

constexpr uint32_t CHUNK_LOAD_COUNT_X =
static_cast<uint32_t>(CHUNK_LOAD_RADIUS_X * 2);

constexpr uint32_t CHUNK_LOAD_COUNT_Y =
static_cast<uint32_t>(CHUNK_LOAD_RADIUS_Y * 2);

constexpr uint32_t CHUNK_LOAD_COUNT_Z =
static_cast<uint32_t>(CHUNK_LOAD_RADIUS_Z * 2);

constexpr uint32_t CHUNK_LOAD_COUNT =
CHUNK_LOAD_COUNT_X *
CHUNK_LOAD_COUNT_Y *
CHUNK_LOAD_COUNT_Z;
