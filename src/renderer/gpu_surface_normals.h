///////////////////////////////////////////////////////////////////////////////
// renderer/gpu_surface_normals.h
// ==============================
//
// Declares GPU resources and operations for generating continuous surface
// normals from procedural density fields.
//
// Normal generation operates on existing GPU mesh vertices. Terrain geometry
// and voxel ownership remain the responsibility of the CPU.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/shader.h"
#include "spatial/spatial_coordinates.h"

#include <cstdint>

struct GpuMesh;
struct SphereDensityField;

/***********************************************************
* GPU Surface Normal Types
************************************************************/

struct GpuSurfaceNormalBuffer
{
	uint32_t handle = 0;
	uint32_t normalCount = 0;
};

struct GpuSurfaceNormalGenerator
{
	ShaderProgram program = {};

	int vertexCountLocation = -1;
	int meshChunkLocation = -1;

	int sphereCenterChunkLocation = -1;
	int sphereCenterLocalLocation = -1;
	int sphereRadiusLocation = -1;
};

/***********************************************************
* GPU Surface Normal Lifecycle
************************************************************/

bool initializeGpuSurfaceNormalGenerator(
	GpuSurfaceNormalGenerator& generator);

void shutdownGpuSurfaceNormalGenerator(
	GpuSurfaceNormalGenerator& generator);

/***********************************************************
* GPU Surface Normal Buffer Lifecycle
************************************************************/

bool createGpuSurfaceNormalBuffer(
	GpuSurfaceNormalBuffer& buffer,
	uint32_t normalCount);

void destroyGpuSurfaceNormalBuffer(
	GpuSurfaceNormalBuffer& buffer);

/***********************************************************
* GPU Surface Normal Generation
************************************************************/

void generateGpuSphereSurfaceNormals(
	const GpuSurfaceNormalGenerator& generator,
	const GpuMesh& mesh,
	const GpuSurfaceNormalBuffer& normalBuffer,
	const ChunkCoord& chunkCoord,
	const SphereDensityField& sphere);