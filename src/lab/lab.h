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

#include "lab/lab_world.h"

#include <glm/mat4x4.hpp>

struct InputState;
struct Renderer;

/***********************************************************
* Lab State
************************************************************/

struct Lab
{
	LabWorld world = {};

	bool showSurfaceReference = true;
	bool showActiveExperiment = true;
	bool showVoxelGrid = false;
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
	Renderer& renderer,
	const glm::mat4& viewProjection);

/***********************************************************
* Lab Debug UI
************************************************************/

void renderLabDebugUi(Lab& lab);