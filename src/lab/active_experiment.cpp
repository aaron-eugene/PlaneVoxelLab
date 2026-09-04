///////////////////////////////////////////////////////////////////////////////
// lab/active_experiment.cpp
// =========================
//
// Implements the compile-time bridge between the lab and the selected active
// experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/active_experiment.h"
#include "lab_world/lab_world.h"

#include "renderer/renderer.h"
#include "renderer/standard_render_settings.h"

#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
#include "experiments/xz_columnar/xz_columnar_experiment.h"

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_MARCHING_TETRAHEDRA
#include "experiments/marching_tetrahedra/marching_tetrahedra_experiment.h"

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif

/***********************************************************
* Active Experiment Lifecycle
************************************************************/

bool initializeActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;
	return true;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	return initializeXZColumnarExperiment(experiment, world);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_MARCHING_TETRAHEDRA
	return initializeMarchingTetrahedraExperiment(experiment, world);

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

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_MARCHING_TETRAHEDRA
	shutdownMarchingTetrahedraExperiment(experiment);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}

/***********************************************************
* Active Experiment Mesh Rebuild
************************************************************/

bool rebuildActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;
	return true;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	return rebuildXZColumnarExperiment(experiment, world);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_MARCHING_TETRAHEDRA
	return rebuildMarchingTetrahedraExperiment(experiment, world);

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
	const glm::mat4& viewProjection)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)renderer;
	(void)renderSettings;
	(void)terrainRenderResources;
	(void)viewProjection;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	renderXZColumnarExperiment(experiment, renderer, renderSettings,
		terrainRenderResources, viewProjection);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_MARCHING_TETRAHEDRA
	renderMarchingTetrahedraExperiment(experiment, renderer);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}

/***********************************************************
* Active Experiment Debug UI
************************************************************/

bool renderActiveExperimentDebugUiContent(
	ActiveExperiment& experiment,
	const LabWorld& world)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;
	return true;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	(void)world;
	return renderXZColumnarExperimentDebugUiContent(
		experiment);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_MARCHING_TETRAHEDRA
	return renderMarchingTetrahedraExperimentDebugUiContent(
		experiment,
		world);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}
