///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_experiment.cpp
// ============================================================
//
// Implements the fragment cleanup terrain experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_experiment.h"

#include "lab_world/lab_world.h"
#include "renderer/renderer.h"
#include "renderer/standard_render_settings.h"
#include "terrain_render/terrain_render_resources.h"

/***********************************************************
* Fragment Cleanup Lifecycle
************************************************************/

bool initializeFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world)
{
	(void)experiment;
	(void)world;

	return true;
}

void shutdownFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment)
{
	(void)experiment;
}

/***********************************************************
* Fragment Cleanup Mesh Building
************************************************************/

bool rebuildFragmentCleanupExperiment(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world)
{
	(void)experiment;
	(void)world;

	return true;
}

/***********************************************************
* Fragment Cleanup Rendering
************************************************************/

void renderFragmentCleanupExperiment(
	const FragmentCleanupExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const ChunkCoord& renderOriginChunk,
	const glm::mat4& viewProjection)
{
	(void)experiment;
	(void)renderer;
	(void)renderSettings;
	(void)terrainRenderResources;
	(void)renderOriginChunk;
	(void)viewProjection;
}

/***********************************************************
* Fragment Cleanup Debug UI
************************************************************/

bool renderFragmentCleanupExperimentDebugUiContent(
	FragmentCleanupExperiment& experiment,
	const LabWorld& world)
{
	(void)experiment;
	(void)world;

	return false;
}