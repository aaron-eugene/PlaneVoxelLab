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
#include "spatial/spatial_coordinates.h"

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>

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
};

struct FragmentCleanupExperiment
{
	std::vector<FragmentCleanupRenderMesh> meshes = {};

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