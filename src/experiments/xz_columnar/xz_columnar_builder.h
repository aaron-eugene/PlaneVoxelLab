///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_builder.h
// =============================================
//
// Declares CPU-side mesh construction for the XZ columnar experiment.
//
// The XZ columnar builder approximates a heightmap density field with planar
// quads. For each X/Z cell in each surface-containing chunk, it builds a local
// bicubic height patch, takes the tangent plane at the patch center, clips the
// resulting planar quad to the owning chunk's Y slab, and emits triangles into
// that chunk's CPU mesh.
//
// This module does not own GPU resources, renderer state, or lab-world data.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "experiments/xz_columnar/xz_columnar_side_region.h"
#include "fields/field_generators.h"
#include "renderer/render_vertex.h"
#include "spatial/spatial_coordinates.h"
#include "surface_map/surface_map.h"

#include <cstdint>
#include <vector>

/***********************************************************
* Columnar Colorization
************************************************************/

enum class XZColumnarColorization
{
	Normal,
	OwnerVoxelY,
};

/***********************************************************
* Columnar Mesh Types
************************************************************/

struct XZColumnarTopPiece
{
	VoxelCoord ownerVoxel = {};

	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
};

struct XZColumnarSideFragment
{
	VoxelCoord ownerVoxel = {};

	XZColumnarSide side =
		XZColumnarSide::PositiveX;

	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
};

struct XZColumnarMesh
{
	ChunkCoord coord = {};

	std::vector<StandardVertex> vertices = {};
	std::vector<uint32_t> indices = {};

	std::vector<XZColumnarTopPiece> topPieces = {};
	std::vector<XZColumnarSideFragment> sideFragments = {};

	// Mesh stats
	bool hasSurfaceCenterHeightRange = false;
	float minSurfaceCenterHeightMeters = 0.0f;
	float maxSurfaceCenterHeightMeters = 0.0f;
};

/***********************************************************
* Columnar Builder Settings
************************************************************/

struct XZColumnarBuildSettings
{
	float derivativeStepMeters = 1.0f;

	XZColumnarColorization colorization =
		XZColumnarColorization::Normal;
};

/***********************************************************
* Columnar Mesh Lifecycle
************************************************************/

void buildXZColumnarMeshes(
	std::vector<XZColumnarMesh>& meshes,
	const HeightmapDensityField& heightmap,
	const SurfaceMap& surfaceMap,
	const XZColumnarBuildSettings& settings);
