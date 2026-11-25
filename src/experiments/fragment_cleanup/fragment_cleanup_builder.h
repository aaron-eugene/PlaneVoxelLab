///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_builder.h
// =======================================================
//
// Declares voxel-owned marching tetrahedra mesh generation for the
// fragment cleanup experiment.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/render_vertex.h"
#include "spatial/spatial_coordinates.h"
#include "terrain_render/terrain_tile_atlas.h"

#include <glm/ext/vector_float3.hpp>

#include <cstdint>
#include <vector>

struct Chunk;

/***********************************************************
* Fragment Cleanup Mesh Types
************************************************************/

struct SurfaceIntersectionKey
{
	SampleCoord sampleA = {};
	SampleCoord sampleB = {};
};

struct SurfaceTriangleKeys
{
	SurfaceIntersectionKey vertices[3] = {};
};

struct FragmentCleanupTopologyTriangle
{
	SurfaceTriangleKeys keys = {};

	glm::vec3 positions[3] = {};

	TerrainTile material = {};
	bool hasMaterial = false;

	uint32_t renderTriangleIndex =
		UINT32_MAX;
};

struct FragmentCleanupTriangleAdjacency
{
	uint32_t neighbors[3] =
	{
		UINT32_MAX,
		UINT32_MAX,
		UINT32_MAX
	};
};

struct FragmentCleanupMaterialBoundary
{
	uint32_t triangleIndex = 0;
	uint32_t edgeIndex = 0;
};

struct FragmentCleanupVoxelRange
{
	VoxelCoord coord = {};

	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
};

struct FragmentCleanupMesh
{
	std::vector<StandardVertex> vertices = {};
	std::vector<uint32_t> indices = {};

	std::vector<FragmentCleanupVoxelRange> voxelRanges = {};

	std::vector<FragmentCleanupTopologyTriangle> topologyTriangles = {};

	std::vector<FragmentCleanupTriangleAdjacency> adjacency = {};

	std::vector<FragmentCleanupMaterialBoundary> materialBoundaries = {};
};

/***********************************************************
* Fragment Cleanup Mesh Interface
************************************************************/

void clearFragmentCleanupMesh(
	FragmentCleanupMesh& mesh);

void appendFragmentCleanupVoxel(
	FragmentCleanupMesh& mesh,
	const Chunk& chunk,
	const VoxelCoord& voxelCoord);

void appendFragmentCleanupExposedFace(
	FragmentCleanupMesh& mesh,
	const Chunk& chunk,
	const VoxelCoord& voxelCoord,
	uint32_t faceIndex);
