
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_cross_chunk.h
// ==========================================================
//
// Matches surface topology edges across chunk boundaries.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <vector>

struct FragmentCleanupRenderMesh;

/***********************************************************
* Cross-Chunk Connection Types
************************************************************/

struct FragmentCleanupCrossChunkEndpoint
{
	uint32_t meshIndex = UINT32_MAX;
	uint32_t triangleIndex = UINT32_MAX;
	uint32_t edgeIndex = UINT32_MAX;
};

struct FragmentCleanupCrossChunkConnection
{
	FragmentCleanupCrossChunkEndpoint a = {};
	FragmentCleanupCrossChunkEndpoint b = {};
};

/***********************************************************
* Cross-Chunk Connection Interface
************************************************************/

bool buildFragmentCleanupCrossChunkConnections(
	std::vector<FragmentCleanupCrossChunkConnection>& connections,
	const std::vector<FragmentCleanupRenderMesh>& meshes);
