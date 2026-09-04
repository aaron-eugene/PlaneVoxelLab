///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_experiment.h
// =================================================
//
// Declares the XZ columnar terrain experiment.
//
// This experiment approximates heightmap terrain with planar quads generated
// from bicubic patch center tangent planes. Geometry is built per owning chunk
// after clipping to each chunk's vertical slab.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "experiments/xz_columnar/xz_columnar_builder.h"
#include "spatial/spatial_coordinates.h"
#include "renderer/gpu_mesh.h"


#include <cstdint>
#include <vector>

struct LabWorld;
struct Renderer;
struct StandardRenderSettings;
struct TerrainRenderResources;

/***********************************************************
* XZ Columnar Debug Stats
************************************************************/

struct XZColumnarDebugStats
{
	uint64_t meshCount = 0;
	uint64_t vertexCount = 0;
	uint64_t indexCount = 0;
	uint64_t topPieceCount = 0;
	uint64_t sideFragmentCount = 0;

	bool hasSurfaceCenterHeightRange = false;
	float minSurfaceCenterHeightMeters = 0.0f;
	float maxSurfaceCenterHeightMeters = 0.0f;
};

/***********************************************************
* XZ Columnar Experiment Types
************************************************************/

struct XZColumnarRenderMesh
{
	ChunkCoord coord = {};

	XZColumnarMesh cpuMesh = {};
	GpuMesh gpuMesh = {};
};

struct XZColumnarExperiment
{
	XZColumnarBuildSettings buildSettings = {};

	std::vector<XZColumnarRenderMesh> meshes = {};

	XZColumnarDebugStats debugStats = {};
};

/***********************************************************
* XZ Columnar Experiment Lifecycle
************************************************************/

bool initializeXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world);

void shutdownXZColumnarExperiment(
	XZColumnarExperiment& experiment);

bool rebuildXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world);

/***********************************************************
* XZ Columnar Experiment Rendering
************************************************************/

void renderXZColumnarExperiment(
	const XZColumnarExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const glm::mat4& viewProjection);

/***********************************************************
* XZ Columnar Experiment Debug UI
************************************************************/

bool renderXZColumnarExperimentDebugUiContent(
	XZColumnarExperiment& experiment);
