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

#include "lab/terrain_render_resources.h"
#include "lab_world/lab_world.h"

#include <glm/mat4x4.hpp>

#define LAB_EXPERIMENT_NONE 0
#define LAB_EXPERIMENT_XZ_COLUMNAR 1	
#define LAB_EXPERIMENT_MARCHING_TETRAHEDRA 2

#define LAB_ACTIVE_EXPERIMENT LAB_EXPERIMENT_XZ_COLUMNAR

struct Renderer;
struct StandardRenderSettings;

// Empty experiment state used when no active experiment is selected.
struct EmptyExperiment
{
};

#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
typedef EmptyExperiment ActiveExperiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
#include "experiments/xz_columnar/xz_columnar_experiment.h"
typedef XZColumnarExperiment ActiveExperiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_MARCHING_TETRAHEDRA
#include "experiments/marching_tetrahedra/marching_tetrahedra_experiment.h"
typedef MarchingTetrahedraExperiment ActiveExperiment;

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
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const glm::mat4& viewProjection);

/***********************************************************
* Active Experiment Debug UI
************************************************************/

bool renderActiveExperimentDebugUiContent(
	ActiveExperiment& experiment,
	const LabWorld& world);
