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
		static_cast<uint32_t>(renderMesh.cpuMesh.vertices.size());

	const uint32_t indexCount =
		static_cast<uint32_t>(renderMesh.cpuMesh.indices.size());

	if (!createGpuMesh(
		renderMesh.gpuMesh,
		renderMesh.cpuMesh.vertices.data(),
		vertexCount,
		renderMesh.cpuMesh.indices.data(),
		indexCount,
		GpuPrimitiveType::Triangles))
	{
		destroyGpuMesh(renderMesh.gpuMesh);
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
	destroyXZColumnarExperimentMeshes(experiment);

	if (world.activeDensityFieldType != LabWorldDensityFieldType::Heightmap)
	{
		return true;
	}

	std::vector<XZColumnarMesh> cpuMeshes = {};

	if (!buildXZColumnarMeshes(
		cpuMeshes,
		world.heightmapField,
		world.surfaceMap,
		experiment.buildSettings))
	{
		return false;
	}

	experiment.meshes.reserve(cpuMeshes.size());

	for (XZColumnarMesh& cpuMesh : cpuMeshes)
	{
		XZColumnarRenderMesh renderMesh = {};
		renderMesh.coord = cpuMesh.coord;
		renderMesh.cpuMesh = std::move(cpuMesh);

		if (!uploadXZColumnarRenderMesh(renderMesh))
		{
			destroyXZColumnarRenderMesh(renderMesh);
			destroyXZColumnarExperimentMeshes(experiment);
			return false;
		}

		experiment.meshes.push_back(std::move(renderMesh));
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
	Renderer& renderer,
	const glm::mat4& viewProjection)
{
	for (const XZColumnarRenderMesh& columnarMesh : experiment.meshes)
	{
		if (columnarMesh.gpuMesh.vertexArray == 0 ||
			columnarMesh.gpuMesh.indexCount == 0)
		{
			continue;
		}

		const glm::mat4 model =
			getChunkModelMatrix(columnarMesh.coord);

		renderMesh(
			columnarMesh.gpuMesh,
			renderer.colorShader,
			model,
			viewProjection);
	}
}

/***********************************************************
* XZ Columnar Experiment Debug UI
************************************************************/

bool renderXZColumnarExperimentDebugUiContent(
	XZColumnarExperiment& experiment,
	const LabWorld& world)
{
	(void)world;

	bool needsRebuild = false;

	ImGui::SeparatorText("XZ Columnar Experiment");

	uint64_t vertexCount = 0;
	uint64_t indexCount = 0;
	uint64_t topPieceCount = 0;
	uint64_t sideFragmentCount = 0;


	for (const XZColumnarRenderMesh& mesh : experiment.meshes)
	{
		vertexCount += mesh.cpuMesh.vertices.size();
		indexCount += mesh.cpuMesh.indices.size();
		
		topPieceCount += mesh.cpuMesh.topPieces.size();
	
		sideFragmentCount +=
			mesh.cpuMesh.sideFragments.size();
	}

	ImGui::Text(
		"Experiment chunks: %zu",
		experiment.meshes.size());

	ImGui::Text(
		"Experiment top pieces: %llu",
		static_cast<unsigned long long>(topPieceCount));

	ImGui::Text(
		"Experiment mesh vertices: %llu",
		static_cast<unsigned long long>(vertexCount));

	ImGui::Text(
		"Experiment mesh indices: %llu",
		static_cast<unsigned long long>(indexCount));

	ImGui::Text(
		"Experiment side fragments: %llu",
		static_cast<unsigned long long>(sideFragmentCount));

	ImGui::Text(
		"Derivative step: %.3f",
		experiment.buildSettings.derivativeStepMeters);

	const char* colorizationName = "Unknown";

	switch (experiment.buildSettings.colorization)
	{
	case XZColumnarColorization::Normal:
	{
		colorizationName = "Normal";
	} break;

	case XZColumnarColorization::OwnerVoxelY:
	{
		colorizationName = "Owner Voxel Y";
	} break;

	default:
	{
		assert(false);
	} break;
	}

	ImGui::Text(
		"Colorization: %s",
		colorizationName);

	if (ImGui::Button("Cycle Colorization"))
	{
		switch (experiment.buildSettings.colorization)
		{
		case XZColumnarColorization::Normal:
		{
			experiment.buildSettings.colorization =
				XZColumnarColorization::OwnerVoxelY;
		} break;

		case XZColumnarColorization::OwnerVoxelY:
		{
			experiment.buildSettings.colorization =
				XZColumnarColorization::Normal;
		} break;

		default:
		{
			assert(false);

			experiment.buildSettings.colorization =
				XZColumnarColorization::Normal;
		} break;
		}

		needsRebuild = true;
	}

	return needsRebuild;
}
