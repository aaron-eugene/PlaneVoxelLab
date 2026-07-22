///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_side_region.h
// =================================================
//
// Declares exposed side-region construction for neighboring XZ columnar
// planar cells.
//
// This module compares shared planar-cell edge profiles, determines which
// cell owns each exposed vertical region, and constructs outward-facing
// polygons for those regions.
//
// It does not slice regions by chunk or voxel Y bounds, emit mesh data, or
// own persistent resources.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "experiments/xz_columnar/xz_columnar_clipping.h"
#include "experiments/xz_columnar/xz_columnar_planar_cell.h"

#include <glm/vec3.hpp>

#include <cstdint>

/***********************************************************
* XZ Columnar Side Region Constants
************************************************************/

inline constexpr uint32_t 
	MAX_XZ_COLUMNAR_SHARED_EDGE_REGIONS = 2;

/***********************************************************
* XZ Columnar Side Region Types
************************************************************/

enum class XZColumnarSide
{
	NegativeX,
	PositiveX,
	NegativeZ,
	PositiveZ,
};

struct XZColumnarEdgeProfile
{
	glm::vec3 start = {};
	glm::vec3 end = {};
};

struct XZColumnarSideRegion
{
	XZColumnarEdgeProfile upper = {};
	XZColumnarEdgeProfile lower = {};

	int32_t ownerRelativeX = 0;
	int32_t ownerRelativeZ = 0;

	XZColumnarSide ownerSide =
		XZColumnarSide::PositiveX;
};

/***********************************************************
* XZ Columnar Side Region Interface
************************************************************/

uint32_t buildXZColumnarSharedEdgeSideRegions(
	XZColumnarSideRegion regions[
		MAX_XZ_COLUMNAR_SHARED_EDGE_REGIONS],
		const XZColumnarPlanarCell& cellA,
		XZColumnarSide sideA,
		const XZColumnarPlanarCell& cellB,
		XZColumnarSide sideB);

XZColumnarClipPolygon getXZColumnarSideRegionPolygon(
	const XZColumnarSideRegion& region);