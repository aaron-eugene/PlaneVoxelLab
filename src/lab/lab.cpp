///////////////////////////////////////////////////////////////////////////////
// lab.cpp
// =======
//
// Implements top-level lab lifecycle, update, rendering, and debug UI.
//
///////////////////////////////////////////////////////////////////////////////

#include "lab/lab.h"

#include "input/input.h"
#include "lab_world/lab_world.h"
#include "renderer/renderer.h"
#include "surface/surface_map.h"
#include "surface_ref/surface_ref.h"

#include <imgui/imgui.h>

#include <cassert>
#include <cstdint>

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
	assert(lab.world.chunks.empty());
	assert(lab.surfaceRef.chunks.empty());

	lab = {};
	lab.showSurfaceReference = true;
	lab.showActiveExperiment = true;
	lab.showVoxelGrid = false;

	if (!initializeLabWorld(lab.world))
	{
		shutdownLab(lab);
		return false;
	}

	if (!initializeSurfaceRef(
		lab.surfaceRef,
		lab.world.chunks,
		lab.world.surfaceMap))
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

	return true;
}

void shutdownLab(
	Lab& lab)
{
	shutdownActiveExperiment(lab.activeExperiment);
	shutdownSurfaceRef(lab.surfaceRef);
	shutdownLabWorld(lab.world);

	lab = {};
}

/***********************************************************
* Lab Update
************************************************************/

void updateLab(
	Lab& lab,
	const InputState& input,
	float deltaSeconds)
{
	if (wasActionPressed(
		input,
		InputAction::ToggleSurfaceReference))
	{
		lab.showSurfaceReference = !lab.showSurfaceReference;
	}

	if (wasActionPressed(
		input,
		InputAction::ToggleActiveExperiment))
	{
		lab.showActiveExperiment = !lab.showActiveExperiment;
	}

	if (wasActionPressed(
		input,
		InputAction::ToggleVoxelGrid))
	{
		lab.showVoxelGrid = !lab.showVoxelGrid;
	}

	if (wasActionPressed(
		input,
		InputAction::RebuildMeshes))
	{
		rebuildSurfaceRef(
			lab.surfaceRef,
			lab.world.chunks,
			lab.world.surfaceMap);
	}

	updateActiveExperiment(
		lab.activeExperiment,
		lab.world,
		deltaSeconds);
}

/***********************************************************
* Lab Rendering
************************************************************/

void renderLab(
	const Lab& lab,
	Renderer& renderer,
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
			viewProjection);
	}
}

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

void renderDebugUiContent(
	Lab& lab)
{
	ImGui::Checkbox(
		"Surface Reference",
		&lab.showSurfaceReference);

	ImGui::Checkbox(
		"Active Experiment",
		&lab.showActiveExperiment);

	ImGui::Checkbox(
		"Voxel Grid",
		&lab.showVoxelGrid);
	/*
	if (ImGui::Button("Rebuild Surface Reference"))
	{
		rebuildSurfaceRef(
			lab.surfaceRef,
			lab.world.chunks);
	}
	*/
	ImGui::Separator();

	uint64_t referenceVertexCount = 0;
	uint64_t referenceIndexCount = 0;

	getSurfaceRefMeshCounts(
		lab.surfaceRef,
		referenceVertexCount,
		referenceIndexCount);

	ImGui::Text(
		"Chunks: %zu",
		lab.world.chunks.size());

	ImGui::Text(
		"Reference chunks: %zu",
		lab.surfaceRef.chunks.size());

	ImGui::Text(
		"Reference vertices: %llu",
		static_cast<unsigned long long>(referenceVertexCount));

	ImGui::Text(
		"Reference indices: %llu",
		static_cast<unsigned long long>(referenceIndexCount));

	uint64_t surfaceVoxelCount = 0;

	for (const SurfaceChunk& surfaceChunk : lab.world.surfaceMap.chunks)
	{
		surfaceVoxelCount += surfaceChunk.voxels.size();
	}

	ImGui::Text(
		"Surface chunks: %zu",
		lab.world.surfaceMap.chunks.size());

	ImGui::Text(
		"Surface voxels: %llu",
		static_cast<unsigned long long>(surfaceVoxelCount));

	LabWorldDensityFieldType selectedFieldType =
		lab.world.activeDensityFieldType;

	const char* selectedFieldName =
		getDensityFieldTypeName(selectedFieldType);

	if (ImGui::BeginCombo(
		"Density Field",
		selectedFieldName))
	{
		if (ImGui::Selectable(
			"Sphere",
			selectedFieldType == LabWorldDensityFieldType::Sphere))
		{
			selectedFieldType = LabWorldDensityFieldType::Sphere;
		}

		if (ImGui::Selectable(
			"Heightmap",
			selectedFieldType == LabWorldDensityFieldType::Heightmap))
		{
			selectedFieldType = LabWorldDensityFieldType::Heightmap;
		}

		ImGui::EndCombo();
	}

	if (selectedFieldType != lab.world.activeDensityFieldType)
	{
		setLabWorldDensityFieldType(
			lab.world,
			selectedFieldType);

		const bool worldRebuilt =
			rebuildLabWorldDensityData(lab.world);

		assert(worldRebuilt);

		const bool surfaceRefRebuilt =
			rebuildSurfaceRef(
				lab.surfaceRef,
				lab.world.chunks,
				lab.world.surfaceMap);

		assert(surfaceRefRebuilt);
	}
}
