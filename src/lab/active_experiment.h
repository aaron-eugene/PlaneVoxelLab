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
#include "terrain_edit/terrain_edit.h"
#include "terrain_render/terrain_render_resources.h"

#include <glm/mat4x4.hpp>

struct Renderer;
struct StandardRenderSettings;

/***********************************************************
* Experiment Selection
************************************************************/

#define LAB_EXPERIMENT_NONE 0
#define LAB_EXPERIMENT_XZ_COLUMNAR 1	
#define LAB_EXPERIMENT_FRAGMENT_CLEANUP 2

#define LAB_ACTIVE_EXPERIMENT LAB_EXPERIMENT_FRAGMENT_CLEANUP

/***********************************************************
* Active Experiment Type
************************************************************/

struct EmptyExperiment
{};

#if LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_NONE
typedef EmptyExperiment ActiveExperiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_XZ_COLUMNAR
#include "experiments/xz_columnar/xz_columnar_experiment.h"
typedef XZColumnarExperiment ActiveExperiment;

#elif LAB_ACTIVE_EXPERIMENT == LAB_EXPERIMENT_FRAGMENT_CLEANUP
#include "experiments/fragment_cleanup/fragment_cleanup_experiment.h"
typedef FragmentCleanupExperiment ActiveExperiment;

#else
#error Unknown LAB_ACTIVE_EXPERIMENT

#endif

/***********************************************************
* Active Experiment Lifecycle
************************************************************/

bool initializeActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits);

void shutdownActiveExperiment(
	ActiveExperiment& experiment);

/***********************************************************
* Active Experiment Mesh Rebuild
************************************************************/

bool rebuildActiveExperiment(
	ActiveExperiment& experiment,
	const LabWorld& world,
	const TerrainEdits& edits);

/***********************************************************
* Active Experiment Rendering
************************************************************/

void renderActiveExperiment(
	const ActiveExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const ChunkCoord& renderOriginChunk,
	const glm::mat4& viewProjection);

/***********************************************************
* Active Experiment Debug UI
************************************************************/

void renderActiveExperimentDebugUiContent(
	ActiveExperiment& experiment,
	const LabWorld& world,
	bool& rebuildRequested);
