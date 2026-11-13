///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_experiment.h
// ==========================================================
//
// Declares the fragment cleanup terrain experiment.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"
#include "renderer/gpu_mesh.h"
#include "renderer/gpu_surface_normals.h"
#include "renderer/surface_shader.h"
#include "spatial/spatial_coordinates.h"

#include <cstdint>
#include <vector>

struct LabWorld;
struct Renderer;
struct StandardRenderSettings;
struct TerrainEdits;
struct TerrainRenderResources;

/***********************************************************
* Fragment Cleanup Debug Stats
************************************************************/

struct FragmentCleanupDebugStats
{
	uint64_t meshCount = 0;
	uint64_t voxelRangeCount = 0;
	uint64_t vertexCount = 0;
	uint64_t indexCount = 0;
};

/***********************************************************
* Fragment Cleanup Experiment Types
************************************************************/

struct FragmentCleanupRenderMesh
{
	ChunkCoord coord = {};

	FragmentCleanupMesh cpuMesh = {};
	GpuMesh gpuMesh = {};

	GpuSurfaceNormalBuffer smoothNormals = {};
	bool hasSmoothNormals = false;
};

struct FragmentCleanupExperiment
{
	std::vector<FragmentCleanupRenderMesh> meshes = {};

	GpuSurfaceNormalGenerator normalGenerator = {};
	SurfaceRenderSettings surfaceSettings = {};

	FragmentCleanupDebugStats debugStats = {};
};

/***********************************************************
* Fragment Cleanup Lifecycle
************************************************************/

bool initializeFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits);

void shutdownFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment);

/***********************************************************
* Fragment Cleanup Mesh Building
************************************************************/

bool rebuildFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits);

/***********************************************************
* Fragment Cleanup Rendering
************************************************************/

void renderFragmentCleanupExperiment(
	const FragmentCleanupExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const ChunkCoord& renderOriginChunk,
	const glm::mat4& viewProjection);

/***********************************************************
* Fragment Cleanup Debug UI
************************************************************/

bool renderFragmentCleanupExperimentDebugUiContent(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world);