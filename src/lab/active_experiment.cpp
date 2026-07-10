///////////////////////////////////////////////////////////////////////////////
// lab/active_experiment.cpp
// =========================
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/active_experiment.h"

#include "renderer/renderer.h"

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
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	return initializePolygonIntersectionExperiment(experiment, world);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	return initializeProxyTilesExperiment(experiment, world);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_COLUMNAR_PATCH
	return initializeColumnarPatchExperiment(experiment, world);
#else
#error Unknown LAB_ACTIVE_EXPERIMENT
#endif
}

void shutdownActiveExperiment(
	ActiveExperiment& experiment)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	shutdownPolygonIntersectionExperiment(experiment);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	shutdownProxyTilesExperiment(experiment);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_COLUMNAR_PATCH
	shutdownColumnarPatchExperiment(experiment);
#else
#error Unknown LAB_ACTIVE_EXPERIMENT
#endif
}

/***********************************************************
* Active Experiment Update
************************************************************/

void updateActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world,
	float deltaTime)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;
	(void)deltaTime;
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	updatePolygonIntersectionExperiment(experiment, world, deltaTime);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	updateProxyTilesExperiment(experiment, world, deltaTime);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_COLUMNAR_PATCH
	updateColumnarPatchExperiment(experiment, world, deltaTime);
#else
#error Unknown LAB_ACTIVE_EXPERIMENT
#endif
}

/***********************************************************
* Active Experiment Mesh Building
************************************************************/

bool buildActiveExperimentMeshes(
	ActiveExperiment& experiment,
	const LabWorld& world)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)world;

	return true;
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	return buildPolygonIntersectionExperimentMeshes(experiment, world);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	return buildProxyTilesExperimentMeshes(experiment, world);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_COLUMNAR_PATCH
	return buildColumnarPatchExperimentMeshes(experiment, world);
#else
#error Unknown LAB_ACTIVE_EXPERIMENT
#endif
}

/***********************************************************
* Active Experiment Rendering
************************************************************/

void renderActiveExperiment(
	const ActiveExperiment& experiment,
	Renderer& renderer,
	const glm::mat4& viewProjection)
{
#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
	(void)experiment;
	(void)renderer;
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	renderPolygonIntersectionExperiment(experiment, renderer);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	renderProxyTilesExperiment(experiment, renderer);
#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_COLUMNAR_PATCH
	renderColumnarPatchExperiment(experiment, renderer);
#else
#error Unknown LAB_ACTIVE_EXPERIMENT
#endif
}
