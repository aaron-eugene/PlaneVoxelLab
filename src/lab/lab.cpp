///////////////////////////////////////////////////////////////////////////////
// lab.cpp
// =======
//
// Implements top-level lab lifecycle, update, rendering, and debug UI.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/lab.h"

#include "input/input.h"
#include "lab/active_experiment.h"
#include "lab_debug/chunk_wireframes.h"
#include "lab_world/lab_world.h"
#include "renderer/renderer.h"
#include "surface_map/surface_map.h"
#include "surface_ref/surface_ref.h"

#include <imgui/imgui.h>

#include <cassert>
#include <cstdint>
#include <cstdio>

/***********************************************************
* Local Helpers
************************************************************/

static void getSurfaceRefMeshCounts(
	const SurfaceRef& surfaceRef,
	uint64_t& vertexCount,
	uint64_t& indexCount)
{
	vertexCount = 0;
	indexCount = 0;

	for (const SurfaceRefChunk& surfaceRefChunk : surfaceRef.chunks)
	{
		vertexCount += surfaceRefChunk.cpuMesh.vertices.size();
		indexCount += surfaceRefChunk.cpuMesh.indices.size();
	}
}

/***********************************************************
* Lab Lifecycle
************************************************************/

bool initializeLab(
	Lab& lab)
{
	lab = {};
	lab.showSurfaceReference = false;
	lab.showActiveExperiment = true;
	lab.showChunkWireframes = false;

	initializeLabWorld(lab.world);

	if (!initializeSurfaceRef(
		lab.surfaceRef,
		lab.world.chunks,
		lab.world.surfaceMap))
	{
		shutdownLab(lab);
		return false;
	}

	if (!initializeChunkWireframes(
		lab.chunkWireframes,
		lab.world.chunks))
	{
		shutdownLab(lab);
		return false;
	}
	
	if (!initializeActiveExperiment(
		lab.activeExperiment,
		lab.world))
	{
		shutdownLab(lab);
		return false;
	}

	if (!createTerrainRenderResources(
		lab.terrainRenderResources))
	{
		shutdownLab(lab);
		return false;
	}

	return true;
}

void shutdownLab(
	Lab& lab)
{
	destroyTerrainRenderResources(lab.terrainRenderResources);
	shutdownActiveExperiment(lab.activeExperiment);
	shutdownChunkWireframes(lab.chunkWireframes);
	shutdownSurfaceRef(lab.surfaceRef);
	shutdownLabWorld(lab.world);

	lab = {};
}

/***********************************************************
* Lab Update
************************************************************/

bool updateLab(
	Lab& lab,
	const InputState& input,
	float deltaSeconds)
{
	(void)deltaSeconds;

	if (wasActionPressed(
		input,
		InputAction::ToggleSurfaceReference))
	{
		lab.showSurfaceReference =
			!lab.showSurfaceReference;
	}

	if (wasActionPressed(
		input,
		InputAction::ToggleActiveExperiment))
	{
		lab.showActiveExperiment =
			!lab.showActiveExperiment;
	}

	if (wasActionPressed(
		input,
		InputAction::ToggleChunkWireframes))
	{
		lab.showChunkWireframes =
			!lab.showChunkWireframes;
	}

	if (wasActionPressed(
		input,
		InputAction::RebuildMeshes))
	{
		if (!rebuildSurfaceRef(
			lab.surfaceRef,
			lab.world.chunks,
			lab.world.surfaceMap))
		{
			std::printf(
				"Failed to rebuild surface reference.\n");

			return false;
		}
	}

	return true;
}

/***********************************************************
* Lab Rendering
************************************************************/

void renderLab(
	const Lab& lab,
	const Renderer& renderer,
	const glm::mat4& viewProjection)
{
	if (lab.showSurfaceReference)
	{
		renderSurfaceRef(
			lab.surfaceRef,
			renderer,
			viewProjection);
	}

	if (lab.showActiveExperiment)
	{
		renderActiveExperiment(
			lab.activeExperiment,
			renderer,
			lab.standardRenderSettings,
			lab.terrainRenderResources,
			viewProjection);
	}

	if (lab.showChunkWireframes)
	{
		renderChunkWireframes(
			lab.chunkWireframes,
			renderer,
			viewProjection);
	}
}

/***********************************************************
* Debug Helpers
************************************************************/

static const char* getDensityFieldTypeName(
	LabWorldDensityFieldType fieldType)
{
	switch (fieldType)
	{
	case LabWorldDensityFieldType::Sphere:
		return "Sphere";

	case LabWorldDensityFieldType::Heightmap:
		return "Heightmap";

	default:
		return "Unknown";
	}
}

static const char* getStandardShadingModeName(
	StandardShadingMode shadingMode)
{
	switch (shadingMode)
	{
	case StandardShadingMode::UnlitVertexColor:
	{
		return "Unlit Vertex Color";
	}

	case StandardShadingMode::LitVertexColor:
	{
		return "Lit Vertex Color";
	}

	case StandardShadingMode::NormalVisualization:
	{
		return "Normal Visualization";
	}

	default:
	{
		assert(false);
		return "Unknown";
	}
	}
}

static void renderStandardRenderSettingsDebugUi(
	StandardRenderSettings& settings)
{
	ImGui::SeparatorText(
		"Standard Rendering");

	const char* selectedModeName =
		getStandardShadingModeName(
			settings.shadingMode);

	if (ImGui::BeginCombo(
		"Shading Mode",
		selectedModeName))
	{
		const bool unlitSelected =
			settings.shadingMode ==
			StandardShadingMode::UnlitVertexColor;

		if (ImGui::Selectable(
			"Unlit Vertex Color",
			unlitSelected))
		{
			settings.shadingMode =
				StandardShadingMode::UnlitVertexColor;
		}

		const bool litSelected =
			settings.shadingMode ==
			StandardShadingMode::LitVertexColor;

		if (ImGui::Selectable(
			"Lit Vertex Color",
			litSelected))
		{
			settings.shadingMode =
				StandardShadingMode::LitVertexColor;
		}

		const bool normalSelected =
			settings.shadingMode ==
			StandardShadingMode::NormalVisualization;

		if (ImGui::Selectable(
			"Normal Visualization",
			normalSelected))
		{
			settings.shadingMode =
				StandardShadingMode::NormalVisualization;
		}

		ImGui::EndCombo();
	}
}

static uint64_t getSurfaceVoxelCount(
	const SurfaceMap& surfaceMap)
{
	uint64_t surfaceVoxelCount = 0;

	for (const SurfaceChunk& surfaceChunk :
		surfaceMap.chunks)
	{
		surfaceVoxelCount +=
			surfaceChunk.voxels.size();
	}

	return surfaceVoxelCount;
}

static void renderLabWorldStatistics(
	const LabWorld& world)
{
	ImGui::SeparatorText(
		"Lab World");

	ImGui::Text(
		"Total chunks: %zu",
		world.chunks.size());

	ImGui::Text(
		"Surface columns: %zu",
		world.surfaceMap.columns.size());

	ImGui::Text(
		"Surface chunks: %zu",
		world.surfaceMap.chunks.size());

	ImGui::Text(
		"Surface voxels: %llu",
		static_cast<unsigned long long>(
			getSurfaceVoxelCount(
				world.surfaceMap)));
}

static void renderSurfaceReferenceStatistics(
	const SurfaceRef& surfaceRef)
{
	uint64_t vertexCount = 0;
	uint64_t indexCount = 0;

	getSurfaceRefMeshCounts(
		surfaceRef,
		vertexCount,
		indexCount);

	ImGui::SeparatorText(
		"Surface Reference");

	ImGui::Text(
		"Reference chunks: %zu",
		surfaceRef.chunks.size());

	ImGui::Text(
		"Reference vertices: %llu",
		static_cast<unsigned long long>(
			vertexCount));

	ImGui::Text(
		"Reference indices: %llu",
		static_cast<unsigned long long>(
			indexCount));
}

static void renderLabComponentToggles(
	Lab& lab)
{
	ImGui::SeparatorText(
		"Visibility");

	ImGui::Checkbox(
		"Surface Reference",
		&lab.showSurfaceReference);

	ImGui::Checkbox(
		"Active Experiment",
		&lab.showActiveExperiment);

	ImGui::Checkbox(
		"Chunk Wireframes",
		&lab.showChunkWireframes);
}

static bool renderDensityFieldSelection(
	LabWorldDensityFieldType& selectedFieldType)
{
	const LabWorldDensityFieldType originalFieldType =
		selectedFieldType;

	const char* selectedFieldName =
		getDensityFieldTypeName(
			selectedFieldType);

	if (ImGui::BeginCombo(
		"Density Field",
		selectedFieldName))
	{
		if (ImGui::Selectable(
			"Sphere",
			selectedFieldType ==
			LabWorldDensityFieldType::Sphere))
		{
			selectedFieldType =
				LabWorldDensityFieldType::Sphere;
		}

		if (ImGui::Selectable(
			"Heightmap",
			selectedFieldType ==
			LabWorldDensityFieldType::Heightmap))
		{
			selectedFieldType =
				LabWorldDensityFieldType::Heightmap;
		}

		ImGui::EndCombo();
	}

	return
		selectedFieldType !=
		originalFieldType;
}

static bool rebuildLabDensityData(
	Lab& lab,
	LabWorldDensityFieldType fieldType)
{
	setLabWorldDensityFieldType(
		lab.world,
		fieldType);

	rebuildLabWorldDensityData(
		lab.world);

	if (!rebuildSurfaceRef(
		lab.surfaceRef,
		lab.world.chunks,
		lab.world.surfaceMap))
	{
		std::printf(
			"Failed to rebuild surface reference after density-field change.\n");

		return false;
	}

	if (!rebuildActiveExperiment(
		lab.activeExperiment,
		lab.world))
	{
		std::printf(
			"Failed to rebuild active experiment after density-field change.\n");

		return false;
	}

	return true;
}

/***********************************************************
* Debug Rendering
************************************************************/

bool renderLabDebugUiContent(
	Lab& lab)
{
	renderLabComponentToggles(
		lab);

	//--------------------------------------------------
	// Density Field
	//--------------------------------------------------

	ImGui::SeparatorText(
		"Density Field");

	LabWorldDensityFieldType selectedFieldType =
		lab.world.activeDensityFieldType;

	if (renderDensityFieldSelection(
		selectedFieldType))
	{
		if (!rebuildLabDensityData(
			lab,
			selectedFieldType))
		{
			return false;
		}
	}

	//--------------------------------------------------
	// Shared Rendering
	//--------------------------------------------------

	renderStandardRenderSettingsDebugUi(
		lab.standardRenderSettings);

	//--------------------------------------------------
	// Statistics
	//--------------------------------------------------

	renderLabWorldStatistics(
		lab.world);

	renderSurfaceReferenceStatistics(
		lab.surfaceRef);

	//--------------------------------------------------
	// Active Experiment
	//--------------------------------------------------

	const bool activeExperimentNeedsRebuild =
		renderActiveExperimentDebugUiContent(
			lab.activeExperiment,
			lab.world);

	if (activeExperimentNeedsRebuild)
	{
		if (!rebuildActiveExperiment(
			lab.activeExperiment,
			lab.world))
		{
			std::printf(
				"Failed to rebuild active experiment after settings change.\n");

			return false;
		}
	}

	return true;
}
