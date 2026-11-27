///////////////////////////////////////////////////////////////////////////////
// app/app_debug_ui.cpp
// ====================
//
// Implements application-level debug windows for the Lab
// and currently active experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "app/app_debug_ui.h"

#include "camera/dev_camera.h"
#include "lab/lab.h"
#include "spatial/spatial_coordinates.h"

#include <glad/glad.h>
#include <imgui/imgui.h>

/***********************************************************
* Debug Window Configuration
************************************************************/

static constexpr float DEBUG_WINDOW_MARGIN_X = 20.0f;
static constexpr float DEBUG_WINDOW_MARGIN_Y = 20.0f;

/***********************************************************
* General Debug Window Helpers
************************************************************/

static void renderApplicationDebugInfo(
	const DevelopmentCamera& camera)
{
	ImGui::Text(
		"OpenGL Version: %s",
		reinterpret_cast<const char*>(
			glGetString(GL_VERSION)));

	ImGui::Separator();

	const WorldPosition& cameraPosition =
		camera.position;

	ImGui::Text(
		"Camera Chunk: %d, %d, %d",
		cameraPosition.chunk.x,
		cameraPosition.chunk.y,
		cameraPosition.chunk.z);

	ImGui::Text(
		"Camera Local: %.2f, %.2f, %.2f",
		cameraPosition.localPosition.x,
		cameraPosition.localPosition.y,
		cameraPosition.localPosition.z);

	const WorldMetricCoordinate cameraX =
		getWorldMetricCoordinate(
			cameraPosition.chunk.x,
			cameraPosition.localPosition.x);

	const WorldMetricCoordinate cameraY =
		getWorldMetricCoordinate(
			cameraPosition.chunk.y,
			cameraPosition.localPosition.y);

	const WorldMetricCoordinate cameraZ =
		getWorldMetricCoordinate(
			cameraPosition.chunk.z,
			cameraPosition.localPosition.z);

	ImGui::Text(
		"Camera Meters: %lld + %.2f, %lld + %.2f, %lld + %.2f",
		static_cast<long long>(cameraX.wholeMeters),
		cameraX.fractionalMeter,
		static_cast<long long>(cameraY.wholeMeters),
		cameraY.fractionalMeter,
		static_cast<long long>(cameraZ.wholeMeters),
		cameraZ.fractionalMeter);

	ImGui::Separator();
}

/***********************************************************
* Debug Window Rendering
************************************************************/

static void renderGeneralLabDebugWindow(
	Lab& lab,
	const DevelopmentCamera& camera,
	const ImGuiViewport& viewport)
{
	ImGui::SetNextWindowPos(
		ImVec2(
			viewport.WorkPos.x + DEBUG_WINDOW_MARGIN_X,
			viewport.WorkPos.y + DEBUG_WINDOW_MARGIN_Y),
		ImGuiCond_FirstUseEver,
		ImVec2(0.0f, 0.0f));

	if (ImGui::Begin(
		"Plane Voxel Lab Debug",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		renderApplicationDebugInfo(camera);

		renderLabDebugUiContent(lab);
	}

	ImGui::End();
}

static void renderActiveExperimentDebugWindow(
	Lab& lab,
	const ImGuiViewport& viewport)
{
	ImGui::SetNextWindowPos(
		ImVec2(
			viewport.WorkPos.x +
			viewport.WorkSize.x -
			DEBUG_WINDOW_MARGIN_X,
			viewport.WorkPos.y +
			DEBUG_WINDOW_MARGIN_Y),
		ImGuiCond_FirstUseEver,
		ImVec2(1.0f, 0.0f));

	if (ImGui::Begin(
		"Active Experiment",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		renderLabExperimentDebugUiContent(lab);
	}

	ImGui::End();
}

/***********************************************************
* Application Debug UI Interface
************************************************************/

void renderAppDebugUi(
	Lab& lab,
	const DevelopmentCamera& camera)
{
	const ImGuiViewport* viewport =
		ImGui::GetMainViewport();

	renderGeneralLabDebugWindow(
		lab,
		camera,
		*viewport);

	renderActiveExperimentDebugWindow(
		lab,
		*viewport);
}
