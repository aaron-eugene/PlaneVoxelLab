///////////////////////////////////////////////////////////////////////////////
// geometry/voxel_topology.h
// =========================
//
// Declares the shared corner, edge, face, and metric conventions for a
// cube-shaped voxel cell.
//
// These conventions define the canonical indexing and winding used by systems
// that interpret voxel geometry.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/ext/vector_float3.hpp>

#include <cstdint>

/***********************************************************
* Voxel Topology Constants
************************************************************/

constexpr uint32_t VOXEL_CORNER_COUNT = 8;
constexpr uint32_t VOXEL_EDGE_COUNT = 12;
constexpr uint32_t VOXEL_FACE_COUNT = 6;

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
