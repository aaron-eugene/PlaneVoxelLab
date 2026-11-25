
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_blend_distance.h
// ================================================================
//
// Calculates approximate surface distances from material boundaries.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "terrain_render/terrain_tile_atlas.h"

#include <cstdint>
#include <vector>

struct FragmentCleanupMesh;
struct FragmentCleanupRenderMesh;
struct FragmentCleanupCrossChunkConnection;

/***********************************************************
* Material Blend Distance Interface
************************************************************/

uint32_t calculateFragmentCleanupBlendDistances(
	std::vector<float>& distances,
	const FragmentCleanupMesh& mesh,
	TerrainTile targetMaterial,
	TerrainTile sourceMaterial,
	float maxDistanceMeters);

uint32_t calculateFragmentCleanupWorldBlendDistances(
	std::vector<std::vector<float>>& distances,
	const std::vector<FragmentCleanupRenderMesh>& meshes,
	const std::vector<FragmentCleanupCrossChunkConnection>& connections,
	TerrainTile targetMaterial,
	TerrainTile sourceMaterial,
	float maxDistanceMeters);

/***********************************************************
* World Material Blend Distance Interface
************************************************************/

uint32_t calculateFragmentCleanupWorldBlendDistances(
	std::vector<std::vector<float>>& distances,
	std::vector<std::vector<uint8_t>>& boundarySeeds,
	const std::vector<FragmentCleanupRenderMesh>& meshes,
	const std::vector<FragmentCleanupCrossChunkConnection>& connections,
	TerrainTile targetMaterial,
	TerrainTile sourceMaterial,
	float maxDistanceMeters);
