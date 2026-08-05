///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_planar_cell.h
// =================================================
//
// Declares planar-cell construction and halo-grid access for the XZ columnar
// experiment.
//
// A planar cell approximates one voxel-sized X/Z heightmap cell using the
// tangent plane at the center of a bicubic Hermite patch. A planar-cell grid
// stores the cells owned by one chunk column together with a one-cell X/Z
// halo used for neighboring-edge comparisons.
//
// This module does not discover exposed side regions, determine voxel-Y
// ownership, emit mesh data, or own persistent renderer resources.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "experiments/xz_columnar/xz_columnar_clipping.h"
#include "fields/field_generators.h"
#include "lab/terrain_tile_atlas.h"
#include "lab_world/lab_world_coordinates.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

/***********************************************************
* XZ Columnar Planar Cell Constants
************************************************************/

inline constexpr uint32_t 
	XZ_COLUMNAR_PLANAR_CELL_HALO_SIZE = 1;

/***********************************************************
* XZ Columnar Planar Cell Types
************************************************************/

struct XZColumnarPlanarCell
{
	glm::vec3 normal = {};
	
	int32_t relativeX = -1;
	int32_t relativeZ = -1;

	glm::vec3 p00 = {};
	glm::vec3 p01 = {};
	glm::vec3 p11 = {};
	glm::vec3 p10 = {};

	glm::vec3 color = {};

	float surfaceHeightMeters = 0.0f;

	TerrainTile surfaceTile =
		TerrainTile::Grass;
};

struct XZColumnarPlanarCellGrid
{
	std::vector<XZColumnarPlanarCell> cells = {};
};

/***********************************************************
* XZ Columnar Planar Cell Interface
************************************************************/

void initializeXZColumnarPlanarCellGrid(
	XZColumnarPlanarCellGrid& grid);

void buildXZColumnarPlanarCellGrid(
	XZColumnarPlanarCellGrid& grid,
	const HeightmapDensityField& heightmap,
	int32_t chunkX,
	int32_t chunkZ,
	float derivativeStepMeters);

const XZColumnarPlanarCell& getXZColumnarPlanarCell(
	const XZColumnarPlanarCellGrid& grid,
	int32_t relativeX,
	int32_t relativeZ);

bool isXZColumnarPlanarCellCoordinateOwned(
	int32_t relativeX,
	int32_t relativeZ);

XZColumnarClipPolygon getXZColumnarPlanarCellTopPolygon(
	const XZColumnarPlanarCell& cell);
