///////////////////////////////////////////////////////////////////////////////
// chunk/chunk_sampling.cpp
// ========================
//
///////////////////////////////////////////////////////////////////////////////

#include "chunk/chunk_sampling.h"

#include "chunk/chunk.h"
#include "fields/density_field.h"
#include "lab_world/lab_world_constants.h"
#include "lab_world/lab_world_coordinates.h"

#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_float3.hpp>

#include <cstdint>

/***********************************************************
* Chunk Sampling Interface
************************************************************/

void sampleChunkDensityField(
	Chunk& chunk,
	const DensityField& field)
{
	for (uint32_t sampleZ = 0;
		sampleZ < CHUNK_SAMPLE_SIZE;
		++sampleZ)
	{
		for (uint32_t sampleY = 0;
			sampleY < CHUNK_SAMPLE_SIZE;
			++sampleY)
		{
			for (uint32_t sampleX = 0;
				sampleX < CHUNK_SAMPLE_SIZE;
				++sampleX)
			{
				const SampleCoord sampleCoord =
				{
					sampleX,
					sampleY,
					sampleZ
				};

				const glm::vec3 sampleLocalPosition =
					getSampleLocalPosition(sampleCoord);

				const glm::dvec3 sampleWorldPosition =
					getWorldPositionFromChunkLocalPosition(
						chunk.coord,
						sampleLocalPosition);

				const float density =
					sampleDensityField(
						field,
						sampleWorldPosition);

				setChunkDensitySample(
					chunk,
					sampleCoord,
					density);
			}
		}
	}
}
