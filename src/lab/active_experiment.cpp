///////////////////////////////////////////////////////////////////////////////
// lab/active_experiment.cpp
// =========================
//
// Implements the compile-time bridge between the lab and the selected active
// experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/active_experiment.h"

#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_FRAGMENT_CLEANUP
#include "experiments/fragment_cleanup/debug/fragment_cleanup_debug_ui.h"
#endif

/***********************************************************
* Active Experiment Lifecycle
************************************************************/

bool initializeActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;
	(void)edits;
	return true;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	return initializeXZColumnarExperiment(
		experiment, 
		world,
		edits);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_FRAGMENT_CLEANUP
	return initializeFragmentCleanupExperiment(
		experiment,
		world,
		edits);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}

void shutdownActiveExperiment(
	ActiveExperiment& experiment)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	shutdownXZColumnarExperiment(experiment);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_FRAGMENT_CLEANUP
	shutdownFragmentCleanupExperiment(experiment);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}

/***********************************************************
* Active Experiment Mesh Rebuild
************************************************************/

bool rebuildActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;
	(void)edits;
	return true;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	return rebuildXZColumnarExperiment(
		experiment, 
		world,
		edits);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_FRAGMENT_CLEANUP
	return rebuildFragmentCleanupExperiment(
		experiment,
		world,
		edits);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}

/***********************************************************
* Active Experiment Rendering
************************************************************/

void renderActiveExperiment(
	const ActiveExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const ChunkCoord& renderOriginChunk,
	const glm::mat4& viewProjection)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)renderer;
	(void)renderSettings;
	(void)terrainRenderResources;
	(void)renderOriginChunk;
	(void)viewProjection;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	renderXZColumnarExperiment(
		experiment,
		renderer,
		renderSettings,
		terrainRenderResources,
		renderOriginChunk,
		viewProjection);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_FRAGMENT_CLEANUP
	renderFragmentCleanupExperiment(
		experiment,
		renderer,
		renderSettings,
		terrainRenderResources,
		renderOriginChunk,
		viewProjection);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}

/***********************************************************
* Active Experiment Debug UI
************************************************************/

void renderActiveExperimentDebugUiContent(
	ActiveExperiment& experiment,
	const LabWorld& world,
	bool& rebuildRequested)
{
	rebuildRequested = false;

#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	(void)world;

	rebuildRequested =
		renderXZColumnarExperimentDebugUiContent(
			experiment);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_FRAGMENT_CLEANUP
	rebuildRequested =
		renderFragmentCleanupExperimentDebugUiContent(
			experiment,
			world);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}
