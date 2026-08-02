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
#include "lab_world/lab_world_coordinates.h"
#include "renderer/gpu_mesh.h"
#include "renderer/renderer.h"
#include "renderer/standard_render_settings.h"

#include <glm/ext/matrix_transform.inl>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_float3.hpp>

#include <imgui/imgui.h>

#include <cassert>
#include <cstdint>
#include <vector>
#include <utility>

/***********************************************************
* File-Local Helpers
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
		destroyGpuMesh(
			renderMesh.gpuMesh);

		return false;
	}

	return true;
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

	return rebuildXZColumnarExperiment(
		experiment,
		world);
}

static void destroyXZColumnarExperimentMeshes(
	XZColumnarExperiment& experiment)
{
	for (XZColumnarRenderMesh& renderMesh : experiment.meshes)
	{
		destroyXZColumnarRenderMesh(renderMesh);
	}

	experiment.meshes.clear();
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

	const bool meshesBuilt =
		buildXZColumnarMeshes(
			cpuMeshes,
			world.heightmapField,
			world.surfaceMap,
			experiment.buildSettings);

	if (!meshesBuilt)
	{
		return false;
	}

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

	return true;
}

/***********************************************************
* XZ Columnar Experiment Update
************************************************************/

void updateXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world,
	float deltaSeconds)
{
	(void)experiment;
	(void)world;
	(void)deltaSeconds;
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
	for (const XZColumnarRenderMesh& columnarMesh :
		experiment.meshes)
	{
		if (columnarMesh.gpuMesh.vertexArray == 0 ||
			columnarMesh.gpuMesh.indexCount == 0)
		{
			continue;
		}

		const glm::mat4 model =
			getChunkModelMatrix(
				columnarMesh.coord);

		renderStandardMesh(
			columnarMesh.gpuMesh,
			renderer.standardShader,
			renderSettings,
			terrainRenderResources.tileAtlas,
			model,
			viewProjection);
	}
}

/***********************************************************
* XZ Columnar Experiment Debug UI
************************************************************/

struct XZColumnarMeshStatistics
{
	uint64_t meshCount = 0;
	uint64_t vertexCount = 0;
	uint64_t indexCount = 0;
	uint64_t topPieceCount = 0;
	uint64_t sideFragmentCount = 0;
};

static XZColumnarMeshStatistics
getXZColumnarMeshStatistics(
	const XZColumnarExperiment& experiment)
{
	XZColumnarMeshStatistics statistics = {};

	statistics.meshCount =
		experiment.meshes.size();

	for (const XZColumnarRenderMesh& mesh :
		experiment.meshes)
	{
		statistics.vertexCount +=
			mesh.cpuMesh.vertices.size();

		statistics.indexCount +=
			mesh.cpuMesh.indices.size();

		statistics.topPieceCount +=
			mesh.cpuMesh.topPieces.size();

		statistics.sideFragmentCount +=
			mesh.cpuMesh.sideFragments.size();
	}

	return statistics;
}

static const char* getXZColumnarColorizationName(
	XZColumnarColorization colorization)
{
	switch (colorization)
	{
	case XZColumnarColorization::Normal:
	{
		return "Normal";
	}

	case XZColumnarColorization::OwnerVoxelY:
	{
		return "Owner Voxel Y";
	}

	default:
	{
		assert(false);
		return "Unknown";
	}
	}
}

static void cycleXZColumnarColorization(
	XZColumnarBuildSettings& settings)
{
	switch (settings.colorization)
	{
	case XZColumnarColorization::Normal:
	{
		settings.colorization =
			XZColumnarColorization::OwnerVoxelY;
	} break;

	case XZColumnarColorization::OwnerVoxelY:
	{
		settings.colorization =
			XZColumnarColorization::Normal;
	} break;

	default:
	{
		assert(false);

		settings.colorization =
			XZColumnarColorization::Normal;
	} break;
	}
}

bool renderXZColumnarExperimentDebugUiContent(
	XZColumnarExperiment& experiment,
	const LabWorld& world)
{
	(void)world;

	bool needsRebuild = false;

	const XZColumnarMeshStatistics statistics =
		getXZColumnarMeshStatistics(
			experiment);

	ImGui::SeparatorText(
		"XZ Columnar Experiment");

	//--------------------------------------------------
	// Geometry Statistics
	//--------------------------------------------------

	ImGui::TextDisabled("Geometry");

	ImGui::Text(
		"Experiment chunks: %llu",
		static_cast<unsigned long long>(
			statistics.meshCount));

	ImGui::Text(
		"Top pieces: %llu",
		static_cast<unsigned long long>(
			statistics.topPieceCount));

	ImGui::Text(
		"Side fragments: %llu",
		static_cast<unsigned long long>(
			statistics.sideFragmentCount));

	ImGui::Text(
		"Mesh vertices: %llu",
		static_cast<unsigned long long>(
			statistics.vertexCount));

	ImGui::Text(
		"Mesh indices: %llu",
		static_cast<unsigned long long>(
			statistics.indexCount));

	ImGui::Spacing();

	//--------------------------------------------------
	// Build Settings
	//--------------------------------------------------

	ImGui::TextDisabled("Build Settings");

	ImGui::Text(
		"Derivative step: %.3f",
		experiment.buildSettings
		.derivativeStepMeters);

	ImGui::Text(
		"Colorization: %s",
		getXZColumnarColorizationName(
			experiment.buildSettings
			.colorization));

	if (ImGui::Button(
		"Cycle Colorization"))
	{
		cycleXZColumnarColorization(
			experiment.buildSettings);

		needsRebuild = true;
	}

	return needsRebuild;
}
