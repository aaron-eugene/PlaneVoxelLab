///////////////////////////////////////////////////////////////////////////////
// lab.h
// =====
//
// Top-level lab module.
//
// The lab owns the terrain experiment workbench state. It coordinates the shared
// lab world, the marching tetrahedra surface reference, and the compile-time
// selected active experiment.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "lab/active_experiment.h"
#include "lab_debug/surface_chunk_wireframes.h"
#include "lab/terrain_render_resources.h"
#include "lab_world/lab_world.h"
#include "renderer/standard_render_settings.h"
#include "surface_ref/surface_ref.h"

#include <glm/mat4x4.hpp>

struct InputState;
struct Renderer;

/***********************************************************
* Lab State
************************************************************/

struct Lab
{	
	LabWorld world = {};

	SurfaceRef surfaceRef = {};
	SurfaceChunkWireframes surfaceChunkWireframes = {};
	ActiveExperiment activeExperiment = {};

	StandardRenderSettings standardRenderSettings = {};
	TerrainRenderResources terrainRenderResources = {};

	bool showSurfaceReference = true;
	bool showActiveExperiment = true;
	bool showSurfaceChunkWireframes = false;
};

/***********************************************************
* Lab Lifecycle
************************************************************/

bool initializeLab(Lab& lab);

void shutdownLab(Lab& lab);

/***********************************************************
* Lab Update
************************************************************/

void updateLab(
	Lab& lab,
	const InputState& input,
	float deltaTime);

/***********************************************************
* Lab Rendering
************************************************************/

void renderLab(
	const Lab& lab,
	const Renderer& renderer,
	const glm::mat4& viewProjection);

/***********************************************************
* Debug Rendering
************************************************************/

void renderDebugUiContent(Lab& lab);
