///////////////////////////////////////////////////////////////////////////////
// lab/active_experiment.h
// =======================
//
// Compile-time bridge between the lab and the selected active experiment.
//
// All experiment modules may remain in the Visual Studio project and continue
// compiling, but only one experiment type is selected as the active experiment
// for the lab build.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "lab_world/lab_world.h"

#include <glm/mat4x4.hpp>

#define LAB_EXPERIMENT_NONE 0
#define LAB_EXPERIMENT_XZ_COLUMNAR 1	
#define LAB_EXPERIMENT_POLYGON_INTERSECTION 2
#define LAB_EXPERIMENT_PROXY_TILES 3

#define LAB_ACTIVE_EXPERIMENT LAB_EXPERIMENT_XZ_COLUMNAR

struct Renderer;

// Placeholder active experiment used while the lab framework is being built.
struct EmptyExperiment
{
};

#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
typedef EmptyExperiment ActiveExperiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
#include "experiments/xz_columnar/xz_columnar_experiment.h"
typedef XZColumnarExperiment ActiveExperiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_POLYGON_INTERSECTION
#include "experiments/polygon_intersection/polygon_intersection_experiment.h"
typedef PolygonIntersectionExperiment ActiveExperiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_PROXY_TILES
#include "experiments/proxy_tiles/proxy_tiles_experiment.h"
typedef ProxyTilesExperiment ActiveExperiment;

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif

/***********************************************************
* Active Experiment Lifecycle
************************************************************/

bool initializeActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world);

void shutdownActiveExperiment(
	ActiveExperiment& experiment);

/***********************************************************
* Active Experiment Update
************************************************************/

void updateActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world,
	float deltaTime);

/***********************************************************
* Active Experiment Mesh Building
************************************************************/

bool rebuildActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world);

/***********************************************************
* Active Experiment Rendering
************************************************************/

void renderActiveExperiment(
	const ActiveExperiment& experiment,
	Renderer& renderer,
	const glm::mat4& viewProjection);

/***********************************************************
* Active Experiment Debug UI
************************************************************/

void renderActiveExperimentDebugUiContent(
	ActiveExperiment& experiment,
	const LabWorld& world);
