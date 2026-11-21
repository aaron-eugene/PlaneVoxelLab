
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_edge_diagnostics.h
// ===================================================================
//
// Declares diagnostic statistics for marching tetrahedra surface edges.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

struct FragmentCleanupMesh;

/***********************************************************
* Surface Edge Diagnostic Types
************************************************************/

struct FragmentCleanupEdgeStats
{
	uint64_t triangleCount = 0;
	uint64_t edgeOccurrences = 0;
	uint64_t uniqueEdges = 0;

	uint64_t singleUseEdges = 0;
	uint64_t doubleUseEdges = 0;
	uint64_t multiUseEdges = 0;

	uint32_t maximumEdgeUse = 0;

	uint64_t boundarySingleUseEdges = 0;
	uint64_t interiorSingleUseEdges = 0;

	uint64_t renderedTriangleCount = 0;
	uint64_t rejectedTriangleCount = 0;

	double elapsedMilliseconds = 0.0;
};

/***********************************************************
* Surface Edge Diagnostic Interface
************************************************************/

FragmentCleanupEdgeStats analyzeFragmentCleanupEdges(
	const FragmentCleanupMesh& mesh);
