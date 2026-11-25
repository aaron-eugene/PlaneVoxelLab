
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_cross_chunk.cpp
// ============================================================
//
// Builds connections between matching cross-chunk surface edges.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_cross_chunk.h"

#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"
#include "experiments/fragment_cleanup/fragment_cleanup_experiment.h"
#include "spatial/spatial_constants.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <tuple>
#include <vector>

/***********************************************************
* Local Types
************************************************************/

namespace
{
	struct WorldSampleKey
	{
		int64_t x = 0;
		int64_t y = 0;
		int64_t z = 0;
	};

	struct WorldIntersectionKey
	{
		WorldSampleKey sampleA = {};
		WorldSampleKey sampleB = {};
	};

	struct WorldSurfaceEdgeKey
	{
		WorldIntersectionKey vertexA = {};
		WorldIntersectionKey vertexB = {};
	};

	struct CrossChunkEdgeOccurrence
	{
		WorldSurfaceEdgeKey key = {};
		FragmentCleanupCrossChunkEndpoint endpoint = {};
	};
}

/***********************************************************
* Key Comparison Helpers
************************************************************/

static bool isWorldSampleKeyLess(
	const WorldSampleKey& a,
	const WorldSampleKey& b)
{
	return
		std::tie(a.x, a.y, a.z) <
		std::tie(b.x, b.y, b.z);
}

static bool isWorldIntersectionKeyLess(
	const WorldIntersectionKey& a,
	const WorldIntersectionKey& b)
{
	if (isWorldSampleKeyLess(a.sampleA, b.sampleA))
	{
		return true;
	}

	if (isWorldSampleKeyLess(b.sampleA, a.sampleA))
	{
		return false;
	}

	return isWorldSampleKeyLess(a.sampleB, b.sampleB);
}

static bool isWorldSurfaceEdgeKeyLess(
	const WorldSurfaceEdgeKey& a,
	const WorldSurfaceEdgeKey& b)
{
	if (isWorldIntersectionKeyLess(a.vertexA, b.vertexA))
	{
		return true;
	}

	if (isWorldIntersectionKeyLess(b.vertexA, a.vertexA))
	{
		return false;
	}

	return isWorldIntersectionKeyLess(a.vertexB, b.vertexB);
}

static bool isCrossChunkEdgeOccurrenceLess(
	const CrossChunkEdgeOccurrence& a,
	const CrossChunkEdgeOccurrence& b)
{
	return isWorldSurfaceEdgeKeyLess(a.key, b.key);
}

static bool isCrossChunkEdgeOccurrenceEqual(
	const CrossChunkEdgeOccurrence& a,
	const CrossChunkEdgeOccurrence& b)
{
	return
		!isWorldSurfaceEdgeKeyLess(a.key, b.key) &&
		!isWorldSurfaceEdgeKeyLess(b.key, a.key);
}

/***********************************************************
* World Key Helpers
************************************************************/

static WorldSampleKey makeWorldSampleKey(
	const ChunkCoord& chunkCoord,
	const SampleCoord& sampleCoord)
{
	return
	{
		static_cast<int64_t>(chunkCoord.x) * CHUNK_SIZE +
			sampleCoord.x,

		static_cast<int64_t>(chunkCoord.y) * CHUNK_SIZE +
			sampleCoord.y,

		static_cast<int64_t>(chunkCoord.z) * CHUNK_SIZE +
			sampleCoord.z
	};
}

static WorldIntersectionKey makeWorldIntersectionKey(
	const ChunkCoord& chunkCoord,
	const SurfaceIntersectionKey& key)
{
	WorldIntersectionKey result = {};

	result.sampleA =
		makeWorldSampleKey(chunkCoord, key.sampleA);

	result.sampleB =
		makeWorldSampleKey(chunkCoord, key.sampleB);

	if (isWorldSampleKeyLess(
		result.sampleB,
		result.sampleA))
	{
		std::swap(result.sampleA, result.sampleB);
	}

	return result;
}

static WorldSurfaceEdgeKey makeWorldSurfaceEdgeKey(
	const ChunkCoord& chunkCoord,
	const SurfaceIntersectionKey& vertexA,
	const SurfaceIntersectionKey& vertexB)
{
	WorldSurfaceEdgeKey result = {};

	result.vertexA =
		makeWorldIntersectionKey(chunkCoord, vertexA);

	result.vertexB =
		makeWorldIntersectionKey(chunkCoord, vertexB);

	if (isWorldIntersectionKeyLess(
		result.vertexB,
		result.vertexA))
	{
		std::swap(result.vertexA, result.vertexB);
	}

	return result;
}

/***********************************************************
* Chunk Boundary Helpers
************************************************************/

static bool isEdgeOnChunkFace(
	const SurfaceIntersectionKey& vertexA,
	const SurfaceIntersectionKey& vertexB)
{
	const SampleCoord samples[4] =
	{
		vertexA.sampleA,
		vertexA.sampleB,
		vertexB.sampleA,
		vertexB.sampleB
	};

	for (uint32_t axis = 0; axis < 3; ++axis)
	{
		for (uint32_t side = 0; side < 2; ++side)
		{
			const uint32_t faceCoordinate =
				side == 0 ? 0 : CHUNK_SIZE;

			bool allOnFace = true;

			for (const SampleCoord& sample : samples)
			{
				const uint32_t coordinate =
					axis == 0 ? sample.x :
					axis == 1 ? sample.y :
					sample.z;

				if (coordinate != faceCoordinate)
				{
					allOnFace = false;
					break;
				}
			}

			if (allOnFace)
			{
				return true;
			}
		}
	}

	return false;
}

/***********************************************************
* Cross-Chunk Connection Interface
************************************************************/

bool buildFragmentCleanupCrossChunkConnections(
	std::vector<FragmentCleanupCrossChunkConnection>& connections,
	const std::vector<FragmentCleanupRenderMesh>& meshes)
{
	connections.clear();

	if (meshes.size() > UINT32_MAX)
	{
		return false;
	}

	std::vector<CrossChunkEdgeOccurrence> edges = {};

	//--------------------------------------------------
	// Collect Unmatched Chunk-Face Edges
	//--------------------------------------------------

	for (uint32_t meshIndex = 0;
		meshIndex < meshes.size();
		++meshIndex)
	{
		const FragmentCleanupRenderMesh& renderMesh =
			meshes[meshIndex];

		const FragmentCleanupMesh& mesh =
			renderMesh.cpuMesh;

		assert(
			mesh.adjacency.size() ==
			mesh.topologyTriangles.size());

		for (uint32_t triangleIndex = 0;
			triangleIndex < mesh.topologyTriangles.size();
			++triangleIndex)
		{
			const FragmentCleanupTopologyTriangle& triangle =
				mesh.topologyTriangles[triangleIndex];

			for (uint32_t edgeIndex = 0;
				edgeIndex < 3;
				++edgeIndex)
			{
				if (mesh.adjacency[triangleIndex]
					.neighbors[edgeIndex] != UINT32_MAX)
				{
					continue;
				}

				const SurfaceIntersectionKey& vertexA =
					triangle.keys.vertices[edgeIndex];

				const SurfaceIntersectionKey& vertexB =
					triangle.keys.vertices[(edgeIndex + 1) % 3];

				if (!isEdgeOnChunkFace(vertexA, vertexB))
				{
					continue;
				}

				CrossChunkEdgeOccurrence occurrence = {};

				occurrence.key =
					makeWorldSurfaceEdgeKey(
						renderMesh.coord,
						vertexA,
						vertexB);

				occurrence.endpoint =
				{
					meshIndex,
					triangleIndex,
					edgeIndex
				};

				edges.push_back(occurrence);
			}
		}
	}

	//--------------------------------------------------
	// Match Identical World-Space Edges
	//--------------------------------------------------

	std::sort(
		edges.begin(),
		edges.end(),
		isCrossChunkEdgeOccurrenceLess);

	uint32_t unmatchedCount = 0;

	size_t index = 0;

	while (index < edges.size())
	{
		size_t nextIndex = index + 1;

		while (nextIndex < edges.size() &&
			isCrossChunkEdgeOccurrenceEqual(
				edges[index],
				edges[nextIndex]))
		{
			++nextIndex;
		}

		const size_t useCount =
			nextIndex - index;

		if (useCount > 2)
		{
			connections.clear();
			return false;
		}

		if (useCount == 2)
		{
			const FragmentCleanupCrossChunkEndpoint& a =
				edges[index].endpoint;

			const FragmentCleanupCrossChunkEndpoint& b =
				edges[index + 1].endpoint;

			if (a.meshIndex == b.meshIndex)
			{
				connections.clear();
				return false;
			}

			connections.push_back({ a, b });
		}
		else
		{
			++unmatchedCount;
		}

		index = nextIndex;
	}

#ifndef NDEBUG
	std::printf(
		"Fragment cleanup cross-chunk topology:\n"
		"  Candidate edges: %zu\n"
		"  Matched connections: %zu\n"
		"  Remaining unmatched: %u\n",
		edges.size(),
		connections.size(),
		unmatchedCount);
#endif

	return true;
}
