///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_adjacency.h
// ===========================================================
//
// Builds chunk-local triangle adjacency from MT intersection keys.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

struct FragmentCleanupMesh;

/***********************************************************
* Fragment Cleanup Adjacency Interface
************************************************************/

bool buildFragmentCleanupAdjacency(
	FragmentCleanupMesh& mesh);
