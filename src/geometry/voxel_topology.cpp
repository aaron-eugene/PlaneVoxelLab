///////////////////////////////////////////////////////////////////////////////
// geometry/voxel_topology.cpp
// ===========================
//
///////////////////////////////////////////////////////////////////////////////

#include "geometry/voxel_topology.h"
#include "lab_world/lab_world_constants.h"

#include <glm/ext/vector_float3.hpp>

#include <cassert>
#include <cstdint>

/***********************************************************
* Voxel Corner Convention
************************************************************
*
* A voxel is identified by its minimum integer grid corner.
*
* For a voxel with minCorner (x, y, z), the corners are:
*
*   7 -------- 6	+Z
*  /|         /|	^
* 4 -------- 5 |	|
* | |        | |	|
* | 3 ------ | 2	---> +X
* |/         |/
* 0 -------- 1
*
* Corner 0 = (x,     y,     z)
* Corner 1 = (x + 1, y,     z)
* Corner 2 = (x + 1, y + 1, z)
* Corner 3 = (x,     y + 1, z)
* Corner 4 = (x,     y,     z + 1)
* Corner 5 = (x + 1, y,     z + 1)
* Corner 6 = (x + 1, y + 1, z + 1)
* Corner 7 = (x,     y + 1, z + 1)
*
************************************************************/

static constexpr VoxelCornerGridOffset VOXEL_CORNER_GRID_OFFSETS[VOXEL_CORNER_COUNT] =
{
	{ 0, 0, 0 }, // 0
	{ 1, 0, 0 }, // 1
	{ 1, 1, 0 }, // 2
	{ 0, 1, 0 }, // 3

	{ 0, 0, 1 }, // 4
	{ 1, 0, 1 }, // 5
	{ 1, 1, 1 }, // 6
	{ 0, 1, 1 }  // 7
};

/***********************************************************
* Voxel Edge Convention
************************************************************
*
*   7 -------- 6	+Z
*  /|         /|	^
* 4 -------- 5 |	|
* | |        | |	|
* | 3 ------ | 2	---> +X
* |/         |/
* 0 -------- 1
*
* Edge 0  = 0 -> 1
* Edge 1  = 1 -> 2
* Edge 2  = 2 -> 3
* Edge 3  = 3 -> 0
*
* Edge 4  = 4 -> 5
* Edge 5  = 5 -> 6
* Edge 6  = 6 -> 7
* Edge 7  = 7 -> 4
*
* Edge 8  = 0 -> 4
* Edge 9  = 1 -> 5
* Edge 10 = 2 -> 6
* Edge 11 = 3 -> 7
*
************************************************************/

static constexpr VoxelEdge VOXEL_EDGES[VOXEL_EDGE_COUNT] =
{
	{ 0, 1 },  // 0
	{ 1, 2 },  // 1
	{ 2, 3 },  // 2
	{ 3, 0 },  // 3

	{ 4, 5 },  // 4
	{ 5, 6 },  // 5
	{ 6, 7 },  // 6
	{ 7, 4 },  // 7

	{ 0, 4 },  // 8
	{ 1, 5 },  // 9
	{ 2, 6 },  // 10
	{ 3, 7 }   // 11
};

/***********************************************************
* Voxel Face Convention
************************************************************
*
* Face corners are wound counter-clockwise when viewed from
* outside the voxel. This means the right-hand rule produces
* outward-facing normals.
*
*   7 -------- 6	+Z
*  /|         /|	^
* 4 -------- 5 |	|
* | |        | |	|
* | 3 ------ | 2	---> +X
* |/         |/
* 0 -------- 1
*
* Face 0 = -X = 0 -> 4 -> 7 -> 3
* Face 1 = +X = 1 -> 2 -> 6 -> 5
*
* Face 2 = -Y = 0 -> 1 -> 5 -> 4
* Face 3 = +Y = 3 -> 7 -> 6 -> 2
*
* Face 4 = -Z = 0 -> 3 -> 2 -> 1
* Face 5 = +Z = 4 -> 5 -> 6 -> 7
*
************************************************************/

static constexpr VoxelFace VOXEL_FACES[VOXEL_FACE_COUNT] =
{
	{ { 0, 4, 7, 3 } }, // -X
	{ { 1, 2, 6, 5 } }, // +X

	{ { 0, 1, 5, 4 } }, // -Y
	{ { 3, 7, 6, 2 } }, // +Y

	{ { 0, 3, 2, 1 } }, // -Z
	{ { 4, 5, 6, 7 } }  // +Z
};

/***********************************************************
* Voxel Corner Topology
************************************************************/

VoxelCornerGridOffset getVoxelCornerGridOffset(
	uint32_t cornerIndex)
{
	assert(cornerIndex < VOXEL_CORNER_COUNT);

	return VOXEL_CORNER_GRID_OFFSETS[cornerIndex];
}

// NOTE: Add table for metric offsets to avoid these casts?
glm::vec3 getVoxelCornerMetricOffset(
	uint32_t cornerIndex)
{
	const VoxelCornerGridOffset offset =
		getVoxelCornerGridOffset(cornerIndex);

	return glm::vec3(
		static_cast<float>(offset.x) * VOXEL_SIZE_METERS,
		static_cast<float>(offset.y) * VOXEL_SIZE_METERS,
		static_cast<float>(offset.z) * VOXEL_SIZE_METERS);
}

glm::vec3 getVoxelCenterMetricOffset()
{
	return glm::vec3(
		VOXEL_SIZE_METERS * 0.5f,
		VOXEL_SIZE_METERS * 0.5f,
		VOXEL_SIZE_METERS * 0.5f);
}

/***********************************************************
* Voxel Edge Topology
************************************************************/

VoxelEdge getVoxelEdge(
	uint32_t edgeIndex)
{
	assert(edgeIndex < VOXEL_EDGE_COUNT);

	return VOXEL_EDGES[edgeIndex];
}

/***********************************************************
* Voxel Face Topology
************************************************************/

VoxelFace getVoxelFace(
	uint32_t faceIndex)
{
	assert(faceIndex < VOXEL_FACE_COUNT);

	return VOXEL_FACES[faceIndex];
}
