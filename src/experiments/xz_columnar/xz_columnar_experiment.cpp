///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_experiment.cpp
// ===================================================
//
// Implements the XZ columnar terrain experiment.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/xz_columnar/xz_columnar_experiment.h"

#include "experiments/xz_columnar/xz_columnar_builder.h"
#include "lab_world/lab_world.h"
#include "renderer/gpu_mesh.h"
#include "renderer/renderer.h"
#include "renderer/standard_render_settings.h"
#include "spatial/spatial_coordinates.h"
#include "terrain_render/terrain_render_resources.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_float3.hpp>

#include <imgui/imgui.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

/***********************************************************
* XZ Columnar Render-Mesh Helpers
************************************************************/

static glm::mat4 getChunkModelMatrix(
	const ChunkCoord& chunkCoord)
{
	const glm::dvec3 chunkWorldMin =
		getChunkWorldMin(chunkCoord);

	return glm::translate(
		glm::mat4(1.0f),
		glm::vec3(chunkWorldMin));
}

static void destroyXZColumnarRenderMesh(
	XZColumnarRenderMesh& renderMesh)
{
	destroyGpuMesh(renderMesh.gpuMesh);

	renderMesh = {};
}

static bool uploadXZColumnarRenderMesh(
	XZColumnarRenderMesh& renderMesh)
{
	assert(renderMesh.gpuMesh.vertexArray == 0);
	assert(renderMesh.gpuMesh.vertexBuffer == 0);
	assert(renderMesh.gpuMesh.indexBuffer == 0);

	assert(
		renderMesh.cpuMesh.vertices.empty() ==
		renderMesh.cpuMesh.indices.empty());

	if (renderMesh.cpuMesh.vertices.empty() ||
		renderMesh.cpuMesh.indices.empty())
	{
		return true;
	}

	const uint32_t vertexCount =
		static_cast<uint32_t>(
			renderMesh.cpuMesh.vertices.size());

	const uint32_t indexCount =
		static_cast<uint32_t>(
			renderMesh.cpuMesh.indices.size());

	if (!createStandardGpuMesh(
		renderMesh.gpuMesh,
		renderMesh.cpuMesh.vertices.data(),
		vertexCount,
		renderMesh.cpuMesh.indices.data(),
		indexCount,
		GpuPrimitiveType::Triangles))
	{
		return false;
	}

	return true;
}

/***********************************************************
* XZ Columnar Display-Mode Helpers
************************************************************/

static XZColumnarVertexColorMode getXZColumnarVertexColorMode(
	XZColumnarDisplayMode displayMode)
{
	switch (displayMode)
	{
	case XZColumnarDisplayMode::TexturedUnlit:
	case XZColumnarDisplayMode::TexturedLit:
	case XZColumnarDisplayMode::NormalVisualization:
		return XZColumnarVertexColorMode::Neutral;

	case XZColumnarDisplayMode::VoxelOwnership:
		return XZColumnarVertexColorMode::OwnerVoxelY;

	default:
		assert(false);
		return XZColumnarVertexColorMode::Neutral;
	}
}

static StandardShadingMode getXZColumnarShadingMode(
	XZColumnarDisplayMode displayMode)
{
	switch (displayMode)
	{
	case XZColumnarDisplayMode::TexturedUnlit:
	case XZColumnarDisplayMode::VoxelOwnership:
		return StandardShadingMode::UnlitVertexColor;

	case XZColumnarDisplayMode::TexturedLit:
		return StandardShadingMode::LitVertexColor;

	case XZColumnarDisplayMode::NormalVisualization:
		return StandardShadingMode::NormalVisualization;

	default:
		assert(false);
		return StandardShadingMode::LitVertexColor;
	}
}

/***********************************************************
* XZ Columnar Debug-Stats Helpers
************************************************************/

static void updateXZColumnarDebugStats(
	XZColumnarExperiment& experiment)
{
	XZColumnarDebugStats& stats =
		experiment.debugStats;

	stats = {};

	stats.meshCount =
		experiment.meshes.size();

	for (const XZColumnarRenderMesh& renderMesh :
		experiment.meshes)
	{
		const XZColumnarMesh& mesh =
			renderMesh.cpuMesh;

		stats.vertexCount +=
			mesh.vertices.size();

		stats.indexCount +=
			mesh.indices.size();

		stats.topPieceCount +=
			mesh.topPieces.size();

		stats.sideFragmentCount +=
			mesh.sideFragments.size();

		if (!mesh.hasSurfaceCenterHeightRange)
		{
			continue;
		}

		if (!stats.hasSurfaceCenterHeightRange)
		{
			stats.minSurfaceCenterHeightMeters =
				mesh.minSurfaceCenterHeightMeters;

			stats.maxSurfaceCenterHeightMeters =
				mesh.maxSurfaceCenterHeightMeters;

			stats.hasSurfaceCenterHeightRange = true;

			continue;
		}

		stats.minSurfaceCenterHeightMeters =
			std::min(
				stats.minSurfaceCenterHeightMeters,
				mesh.minSurfaceCenterHeightMeters);

		stats.maxSurfaceCenterHeightMeters =
			std::max(
				stats.maxSurfaceCenterHeightMeters,
				mesh.maxSurfaceCenterHeightMeters);
	}
}

/***********************************************************
* XZ Columnar Experiment Lifecycle
************************************************************/

bool initializeXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world)
{
	assert(experiment.meshes.empty());

	experiment = {};

	experiment.buildSettings.derivativeStepMeters = VOXEL_SIZE_METERS;
	
	experiment.buildSettings.vertexColorMode =
		getXZColumnarVertexColorMode(
			experiment.displayMode);

	return rebuildXZColumnarExperiment(
		experiment,
		world);
}

static void destroyXZColumnarExperimentMeshes(
	XZColumnarExperiment& experiment)
{
	for (XZColumnarRenderMesh& renderMesh :
		experiment.meshes)
	{
		destroyXZColumnarRenderMesh(
			renderMesh);
	}

	experiment.meshes.clear();
	experiment.debugStats = {};
}

void shutdownXZColumnarExperiment(
	XZColumnarExperiment& experiment)
{
	destroyXZColumnarExperimentMeshes(experiment);

	experiment = {};
}

bool rebuildXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world)
{
	destroyXZColumnarExperimentMeshes(
		experiment);

	if (world.activeDensityFieldType !=
		LabWorldDensityFieldType::Heightmap)
	{
		return true;
	}

	std::vector<XZColumnarMesh> cpuMeshes = {};

	buildXZColumnarMeshes(
		cpuMeshes,
		world.heightmapField,
		world.surfaceMap,
		experiment.buildSettings);

	experiment.meshes.reserve(
		cpuMeshes.size());

	for (XZColumnarMesh& cpuMesh :
		cpuMeshes)
	{
		XZColumnarRenderMesh renderMesh = {};

		renderMesh.coord =
			cpuMesh.coord;

		renderMesh.cpuMesh =
			std::move(cpuMesh);

		if (!uploadXZColumnarRenderMesh(
			renderMesh))
		{
			destroyXZColumnarRenderMesh(
				renderMesh);

			destroyXZColumnarExperimentMeshes(
				experiment);

			return false;
		}

		experiment.meshes.push_back(
			std::move(renderMesh));
	}

	updateXZColumnarDebugStats(experiment);

	return true;
}

/***********************************************************
* XZ Columnar Experiment Rendering
************************************************************/

void renderXZColumnarExperiment(
	const XZColumnarExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const glm::mat4& viewProjection)
{
	StandardRenderSettings experimentRenderSettings =
		renderSettings;

	experimentRenderSettings.shadingMode =
		getXZColumnarShadingMode(
			experiment.displayMode);
	
	for (const XZColumnarRenderMesh& renderMesh :
		experiment.meshes)
	{
		if (renderMesh.gpuMesh.vertexArray == 0 ||
			renderMesh.gpuMesh.indexCount == 0)
		{
			continue;
		}

		const glm::mat4 model =
			getChunkModelMatrix(
				renderMesh.coord);

		renderStandardMesh(
			renderer,
			renderMesh.gpuMesh,
			experimentRenderSettings,
			terrainRenderResources.tileAtlas,
			model,
			viewProjection);
	}
}

/***********************************************************
* XZ Columnar Experiment Debug-UI Helpers
************************************************************/

static const char* getXZColumnarDisplayModeName(
	XZColumnarDisplayMode displayMode)
{
	switch (displayMode)
	{
	case XZColumnarDisplayMode::TexturedUnlit:
		return "Textured Unlit";

	case XZColumnarDisplayMode::TexturedLit:
		return "Textured Lit";

	case XZColumnarDisplayMode::NormalVisualization:
		return "Normal Visualization";

	case XZColumnarDisplayMode::VoxelOwnership:
		return "Voxel Ownership";

	default:
		assert(false);
		return "Unknown";
	}
}

static XZColumnarDisplayMode getNextXZColumnarDisplayMode(
	XZColumnarDisplayMode displayMode)
{
	switch (displayMode)
	{
	case XZColumnarDisplayMode::TexturedUnlit:
		return XZColumnarDisplayMode::TexturedLit;

	case XZColumnarDisplayMode::TexturedLit:
		return XZColumnarDisplayMode::NormalVisualization;

	case XZColumnarDisplayMode::NormalVisualization:
		return XZColumnarDisplayMode::VoxelOwnership;

	case XZColumnarDisplayMode::VoxelOwnership:
		return XZColumnarDisplayMode::TexturedUnlit;

	default:
		assert(false);
		return XZColumnarDisplayMode::TexturedLit;
	}
}

/***********************************************************
* XZ Columnar Experiment Debug-UI
************************************************************/

bool renderXZColumnarExperimentDebugUiContent(
	XZColumnarExperiment& experiment)
{
	bool needsRebuild = false;

	const XZColumnarDebugStats& stats =
		experiment.debugStats;

	ImGui::SeparatorText(
		"XZ Columnar Experiment");

	//--------------------------------------------------
	// Geometry Stats
	//--------------------------------------------------

	ImGui::TextDisabled("Geometry");

	ImGui::Text(
		"Experiment chunks: %llu",
		static_cast<unsigned long long>(
			stats.meshCount));

	ImGui::Text(
		"Top pieces: %llu",
		static_cast<unsigned long long>(
			stats.topPieceCount));

	ImGui::Text(
		"Side fragments: %llu",
		static_cast<unsigned long long>(
			stats.sideFragmentCount));

	ImGui::Text(
		"Mesh vertices: %llu",
		static_cast<unsigned long long>(
			stats.vertexCount));

	ImGui::Text(
		"Mesh indices: %llu",
		static_cast<unsigned long long>(
			stats.indexCount));

	ImGui::Spacing();

	//--------------------------------------------------
	// Terrain Stats
	//--------------------------------------------------

	ImGui::TextDisabled("Terrain");

	if (stats.hasSurfaceCenterHeightRange)
	{
		ImGui::Text(
			"Surface center height: %.2f to %.2f m",
			stats.minSurfaceCenterHeightMeters,
			stats.maxSurfaceCenterHeightMeters);

		ImGui::Text(
			"Center height span: %.2f m",
			stats.maxSurfaceCenterHeightMeters -
			stats.minSurfaceCenterHeightMeters);
	}
	else
	{
		ImGui::TextDisabled(
			"Surface height: unavailable");
	}

	ImGui::Spacing();

	//--------------------------------------------------
	// Display Mode
	//--------------------------------------------------

	ImGui::TextDisabled("Display");

	ImGui::Text(
		"Mode: %s",
		getXZColumnarDisplayModeName(
			experiment.displayMode));

	if (ImGui::Button(
		"Cycle Display Mode"))
	{
		const XZColumnarVertexColorMode
			previousVertexColorMode =
			experiment.buildSettings.vertexColorMode;

		experiment.displayMode =
			getNextXZColumnarDisplayMode(
				experiment.displayMode);

		experiment.buildSettings.vertexColorMode =
			getXZColumnarVertexColorMode(
				experiment.displayMode);

		if (experiment.buildSettings.vertexColorMode !=
			previousVertexColorMode)
		{
			needsRebuild = true;
		}
	}

	ImGui::Spacing();

	//--------------------------------------------------
	// Build Settings
	//--------------------------------------------------

	ImGui::TextDisabled("Build Settings");

	ImGui::Text(
		"Derivative step: %.3f",
		experiment.buildSettings
		.derivativeStepMeters);

	return needsRebuild;
}
