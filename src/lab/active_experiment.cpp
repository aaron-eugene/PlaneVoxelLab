///////////////////////////////////////////////////////////////////////////////
// lab/active_experiment.cpp
// =========================
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/active_experiment.h"
#include "lab_world/lab_world.h"

#include "renderer/renderer.h"

#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
#include "experiments/xz_columnar/xz_columnar_experiment.h"

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
//#include

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
//#include

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

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	return initializePolygonIntersectionExperiment(experiment, world);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	return initializeProxyTilesExperiment(experiment, world);

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

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	shutdownPolygonIntersectionExperiment(experiment);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	shutdownProxyTilesExperiment(experiment);

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

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	updateXZColumnarExperiment(experiment, world, deltaTime);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	updatePolygonIntersectionExperiment(experiment, world, deltaTime);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	updateProxyTilesExperiment(experiment, world, deltaTime);

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

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	return buildPolygonIntersectionExperimentMeshes(experiment, world);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	return buildProxyTilesExperimentMeshes(experiment, world);

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
	(void)viewProjection;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
	renderXZColumnarExperiment(experiment, renderer, viewProjection);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	renderPolygonIntersectionExperiment(experiment, renderer);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	renderProxyTilesExperiment(experiment, renderer);

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
	return renderXZColumnarExperimentDebugUiContent(
		experiment,
		world);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
	return renderPolygonIntersectionExperimentDebugUiContent(
		experiment,
		world);

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
	return renderProxyTilesExperimentDebugUiContent(
		experiment,
		world);

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif
}
