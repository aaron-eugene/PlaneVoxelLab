///////////////////////////////////////////////////////////////////////////////
// surface_ref/marching_tetrahedra_tetrahedra_tables.h
// ===================================================
//
// Defines marching tetrahedra lookup tables for the lab world's voxel corner
// convention.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/***********************************************************
* Marching Tetrahedra Constants
************************************************************/

static constexpr uint32_t MARCHING_TETRAHEDRA_PER_VOXEL = 6;
static constexpr uint32_t MARCHING_TETRAHEDRON_CORNER_COUNT = 4;
static constexpr uint32_t MARCHING_TETRAHEDRON_EDGE_COUNT = 6;

/***********************************************************
* Marching Tetrahedra Tables
************************************************************/

static constexpr uint32_t MARCHING_TETRAHEDRA_CORNERS
[MARCHING_TETRAHEDRA_PER_VOXEL]
[MARCHING_TETRAHEDRON_CORNER_COUNT] =
{
	{ 0, 1, 2, 6 },
	{ 0, 2, 3, 6 },
	{ 0, 3, 7, 6 },
	{ 0, 7, 4, 6 },
	{ 0, 4, 5, 6 },
	{ 0, 5, 1, 6 }
};

static constexpr uint32_t MARCHING_TETRAHEDRON_EDGES
[MARCHING_TETRAHEDRON_EDGE_COUNT]
[2] =
{
	{ 0, 1 },
	{ 0, 2 },
	{ 0, 3 },
	{ 1, 2 },
	{ 1, 3 },
	{ 2, 3 }
};
