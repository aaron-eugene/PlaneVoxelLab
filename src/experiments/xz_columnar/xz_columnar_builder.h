///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_builder.h
// =============================================
//
// Declares CPU-side mesh construction for the columnar patch experiment.
//
// The columnar patch builder approximates a heightmap density field with planar
// quads. For each X/Z cell in each surface-containing chunk, it builds a local
// bicubic height patch, takes the tangent plane at the patch center, clips the
// resulting planar quad to the owning chunk's Y slab, and emits triangles into
// that chunk's CPU mesh.
//
// This module does not own GPU resources, renderer state, or lab-world data.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "fields/field_generators.h"
#include "lab_world/lab_world_coordinates.h"
#include "renderer/render_vertex.h"
#include "surface/surface_map.h"

#include <cstdint>
#include <vector>

/***********************************************************
* Columnar Patch Colorization
************************************************************/

enum class XZColumnarColorization
{
	Normal,
	OwnerVoxelY,
};

/***********************************************************
* Columnar Patch Mesh Types
************************************************************/

struct XZColumnarPiece
{
	VoxelCoord ownerVoxel = {};

	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
};

struct XZColumnarMesh
{
	ChunkCoord coord = {};

	std::vector<ColoredVertex> vertices = {};
	std::vector<uint32_t> indices = {};

	std::vector<XZColumnarPiece> pieces = {};
};

/***********************************************************
* Columnar Patch Builder Settings
************************************************************/

struct XZColumnarBuildSettings
{
	float derivativeStepMeters = 1.0f;

	XZColumnarColorization colorization =
		XZColumnarColorization::Normal;
};

/***********************************************************
* Columnar Patch Mesh Lifecycle
************************************************************/

void clearXZColumnarMeshes(
	std::vector<XZColumnarMesh>& meshes);

/***********************************************************
* Columnar Patch Mesh Building
************************************************************/

bool buildXZColumnarMeshes(
	std::vector<XZColumnarMesh>& meshes,
	const HeightmapDensityField& heightmap,
	const SurfaceMap& surfaceMap,
	const XZColumnarBuildSettings& settings);
