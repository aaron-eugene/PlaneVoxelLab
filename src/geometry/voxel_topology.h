///////////////////////////////////////////////////////////////////////////////
// geometry/voxel_topology.h
// =========================
//
// Declares topology helpers for a single cube-shaped voxel cell.
//
// This module defines the shared corner, edge, and face conventions for one
// voxel. Surface extraction, debug drawing, and chunk code use these conventions
// when interpreting voxel-local corners, edges, and faces.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "lab_world/lab_world_constants.h"

#include <glm/ext/vector_float3.hpp>

#include <cstdint>

/***********************************************************
* Voxel Topology Types
************************************************************/

struct VoxelCornerGridOffset
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;
};

struct VoxelEdge
{
	uint32_t cornerA = 0;
	uint32_t cornerB = 0;
};

struct VoxelFace
{
	uint32_t corners[4] = {};
};

/***********************************************************
* Voxel Corner Topology
************************************************************/

VoxelCornerGridOffset getVoxelCornerGridOffset(
	uint32_t cornerIndex);

glm::vec3 getVoxelCornerMetricOffset(
	uint32_t cornerIndex);

glm::vec3 getVoxelCenterMetricOffset();

/***********************************************************
* Voxel Edge Topology
************************************************************/

VoxelEdge getVoxelEdge(
	uint32_t edgeIndex);

/***********************************************************
* Voxel Face Topology
************************************************************/

VoxelFace getVoxelFace(
	uint32_t faceIndex);
