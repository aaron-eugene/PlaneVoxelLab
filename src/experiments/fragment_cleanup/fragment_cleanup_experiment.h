///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_experiment.h
// ==========================================================
//
// Declares the fragment cleanup terrain experiment.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "spatial/spatial_coordinates.h"

#include <glm/mat4x4.hpp>

struct LabWorld;
struct Renderer;
struct StandardRenderSettings;
struct TerrainRenderResources;

/***********************************************************
* Fragment Cleanup Experiment State
************************************************************/

struct FragmentCleanupExperiment
{};

/***********************************************************
* Fragment Cleanup Lifecycle
************************************************************/

bool initializeFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world);

void shutdownFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment);

/***********************************************************
* Fragment Cleanup Mesh Building
************************************************************/

bool rebuildFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world);

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