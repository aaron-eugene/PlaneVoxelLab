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

#include <cstdint>
#include <vector>

struct Chunk;
struct SurfaceChunk;

/***********************************************************
* Fragment Cleanup Mesh Types
************************************************************/

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

void buildFragmentCleanupMesh(
	FragmentCleanupMesh& mesh,
	const Chunk& chunk,
	const SurfaceChunk& surfaceChunk);
