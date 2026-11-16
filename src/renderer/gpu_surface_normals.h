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

struct GpuHeightmapNoiseConfiguration;
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

struct GpuSphereNormalProgram
{
	ShaderProgram program = {};

	int vertexCountLocation = -1;
	int meshChunkLocation = -1;

	int centerChunkLocation = -1;
	int centerLocalLocation = -1;
	int radiusLocation = -1;
};

struct GpuHeightmapNormalProgram
{
	ShaderProgram program = {};

	uint32_t octaveBuffer = 0;
	bool configured = false;

	int vertexCountLocation = -1;
	int meshChunkLocation = -1;

	int octaveCountLocation = -1;
	int seedLocation = -1;
	int totalAmplitudeLocation = -1;

	int heightAmplitudeLocation = -1;
	int gradientStepLocation = -1;
};

struct GpuSurfaceNormalGenerator
{
	GpuSphereNormalProgram sphere = {};
	GpuHeightmapNormalProgram heightmap = {};
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

bool prepareGpuHeightmapSurfaceNormals(
	GpuSurfaceNormalGenerator& generator,
	const GpuHeightmapNoiseConfiguration& configuration);

void generateGpuHeightmapSurfaceNormals(
	const GpuSurfaceNormalGenerator& generator,
	const GpuMesh& mesh,
	const GpuSurfaceNormalBuffer& normalBuffer,
	const ChunkCoord& chunkCoord);