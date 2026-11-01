///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_tables.h
// ========================================================
//
// Defines marching tetrahedra lookup tables for fragment cleanup.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Fragment Cleanup Tetrahedra Constants
************************************************************/

static constexpr uint32_t FRAGMENT_CLEANUP_TETRAHEDRA_PER_VOXEL = 6;
static constexpr uint32_t FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT = 4;

/***********************************************************
* Fragment Cleanup Tetrahedra Tables
************************************************************/

static constexpr uint32_t FRAGMENT_CLEANUP_TETRAHEDRA_CORNERS
[FRAGMENT_CLEANUP_TETRAHEDRA_PER_VOXEL]
[FRAGMENT_CLEANUP_TETRAHEDRON_CORNER_COUNT] =
{
	{ 0, 1, 2, 6 },
	{ 0, 2, 3, 6 },
	{ 0, 3, 7, 6 },
	{ 0, 7, 4, 6 },
	{ 0, 4, 5, 6 },
	{ 0, 5, 1, 6 }
};