///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_material_boundaries.h
// =====================================================================
//
// Identifies material transitions between adjacent surface triangles.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

struct FragmentCleanupMesh;

/***********************************************************
* Material Boundary Interface
************************************************************/

void buildFragmentCleanupMaterialBoundaries(
	FragmentCleanupMesh& mesh);
