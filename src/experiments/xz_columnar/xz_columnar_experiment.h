///////////////////////////////////////////////////////////////////////////////
// experiments/xz_columnar/xz_columnar_experiment.h
// =================================================
//
// Declares the XZ columnar terrain experiment.
//
// This experiment approximates heightmap terrain with planar quads generated
// from bicubic patch center tangent planes. Geometry is built per owning chunk
// after clipping to each chunk's vertical slab.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "experiments/xz_columnar/xz_columnar_builder.h"
#include "lab/terrain_render_resources.h"
#include "lab_world/lab_world_coordinates.h"
#include "renderer/gpu_mesh.h"

#include <glm/ext/matrix_float4x4.hpp>

#include <vector>

struct LabWorld;
struct Renderer;
struct StandardRenderSettings;

/***********************************************************
* XZ Columnar Experiment Types
************************************************************/

struct XZColumnarRenderMesh
{
	ChunkCoord coord = {};

	XZColumnarMesh cpuMesh = {};
	GpuMesh gpuMesh = {};
};

struct XZColumnarExperiment
{
	XZColumnarBuildSettings buildSettings = {};

	std::vector<XZColumnarRenderMesh> meshes = {};
};

/***********************************************************
* XZ Columnar Experiment Lifecycle
************************************************************/

bool initializeXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world);

void shutdownXZColumnarExperiment(
	XZColumnarExperiment& experiment);

bool rebuildXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world);

/***********************************************************
* XZ Columnar Experiment Update
************************************************************/

void updateXZColumnarExperiment(
	XZColumnarExperiment& experiment,
	const LabWorld& world,
	float deltaSeconds);

/***********************************************************
* XZ Columnar Experiment Rendering
************************************************************/

void renderXZColumnarExperiment(
	const XZColumnarExperiment& experiment,
	const Renderer& renderer,
	const StandardRenderSettings& renderSettings,
	const TerrainRenderResources& terrainRenderResources,
	const glm::mat4& viewProjection);

/***********************************************************
* XZ Columnar Experiment Debug UI
************************************************************/

bool renderXZColumnarExperimentDebugUiContent(
	XZColumnarExperiment& experiment,
	const LabWorld& world);
