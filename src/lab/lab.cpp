///////////////////////////////////////////////////////////////////////////////
// lab.cpp
// =======
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/lab.h"

#include "input/input.h"
#include "renderer/renderer.h"

#include <imgui/imgui.h>

/***********************************************************
* Lab Lifecycle
************************************************************/

bool initializeLab(Lab& lab)
{
	lab = {};

	lab.showSurfaceReference = true;
	lab.showActiveExperiment = true;
	lab.showVoxelGrid = false;

	if (!initializeLabWorld(lab.world))
	{
		shutdownLab(lab);
		return false;
	}

	return true;
}

void shutdownLab(Lab& lab)
{
	shutdownLabWorld(lab.world);

	lab = {};
}

/***********************************************************
* Lab Update
************************************************************/

void updateLab(
	Lab& lab,
	const InputState& input,
	float deltaTime)
{
	(void)lab;
	(void)input;
	(void)deltaTime;

	// Surface reference and active experiment updates will be called here.
}

/***********************************************************
* Lab Rendering
************************************************************/

void renderLab(
	const Lab& lab,
	Renderer& renderer,
	const glm::mat4& viewProjection)
{
	(void)lab;
	(void)renderer;
	(void)viewProjection;

	// Surface reference and active experiment rendering will be called here.
}

/***********************************************************
* Lab Debug UI
************************************************************/

void renderLabDebugUi(Lab& lab)
{
	ImGui::Text("Lab initialized.");

	ImGui::Checkbox(
		"Show Surface Reference",
		&lab.showSurfaceReference);

	ImGui::Checkbox(
		"Show Active Experiment",
		&lab.showActiveExperiment);

	ImGui::Checkbox(
		"Show Voxel Grid",
		&lab.showVoxelGrid);
}
