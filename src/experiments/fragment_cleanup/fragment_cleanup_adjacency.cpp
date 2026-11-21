
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_adjacency.cpp
// =============================================================
//
// Builds triangle neighbor references from shared surface-edge keys.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_adjacency.h"

#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

/***********************************************************
* Local Types
************************************************************/

namespace
{
	struct SurfaceEdgeOccurrence
	{
		SurfaceIntersectionKey vertexA = {};
		SurfaceIntersectionKey vertexB = {};

		uint32_t triangleIndex = 0;
		uint32_t edgeIndex = 0;
	};
}

/***********************************************************
* Key Comparison Helpers
************************************************************/

static bool isSampleCoordLess(
	const SampleCoord& a,
	const SampleCoord& b)
{
	if (a.x != b.x)
	{
		return a.x < b.x;
	}

	if (a.y != b.y)
	{
		return a.y < b.y;
	}

	return a.z < b.z;
}

static bool isIntersectionKeyLess(
	const SurfaceIntersectionKey& a,
	const SurfaceIntersectionKey& b)
{
	if (isSampleCoordLess(a.sampleA, b.sampleA))
	{
		return true;
	}

	if (isSampleCoordLess(b.sampleA, a.sampleA))
	{
		return false;
	}

	return isSampleCoordLess(a.sampleB, b.sampleB);
}

static bool isSurfaceEdgeLess(
	const SurfaceEdgeOccurrence& a,
	const SurfaceEdgeOccurrence& b)
{
	if (isIntersectionKeyLess(a.vertexA, b.vertexA))
	{
		return true;
	}

	if (isIntersectionKeyLess(b.vertexA, a.vertexA))
	{
		return false;
	}

	return isIntersectionKeyLess(a.vertexB, b.vertexB);
}

static bool isSurfaceEdgeEqual(
	const SurfaceEdgeOccurrence& a,
	const SurfaceEdgeOccurrence& b)
{
	return
		!isSurfaceEdgeLess(a, b) &&
		!isSurfaceEdgeLess(b, a);
}

/***********************************************************
* Edge Occurrence Helpers
************************************************************/

static SurfaceEdgeOccurrence makeSurfaceEdgeOccurrence(
	const SurfaceIntersectionKey& vertexA,
	const SurfaceIntersectionKey& vertexB,
	uint32_t triangleIndex,
	uint32_t edgeIndex)
{
	SurfaceEdgeOccurrence occurrence = {};

	if (isIntersectionKeyLess(vertexB, vertexA))
	{
		occurrence.vertexA = vertexB;
		occurrence.vertexB = vertexA;
	}
	else
	{
		occurrence.vertexA = vertexA;
		occurrence.vertexB = vertexB;
	}

	occurrence.triangleIndex = triangleIndex;
	occurrence.edgeIndex = edgeIndex;

	return occurrence;
}

/***********************************************************
* Fragment Cleanup Adjacency Interface
************************************************************/

bool buildFragmentCleanupAdjacency(
	FragmentCleanupMesh& mesh)
{
	mesh.adjacency.clear();

	if (mesh.topologyTriangles.size() >
		std::numeric_limits<uint32_t>::max())
	{
		return false;
	}

	const uint32_t triangleCount =
		static_cast<uint32_t>(
			mesh.topologyTriangles.size());

	mesh.adjacency.resize(triangleCount);

	std::vector<SurfaceEdgeOccurrence> edges = {};

	edges.reserve(
		static_cast<size_t>(triangleCount) * 3);

	for (uint32_t triangleIndex = 0;
		triangleIndex < triangleCount;
		++triangleIndex)
	{
		const SurfaceTriangleKeys& triangle =
			mesh.topologyTriangles[triangleIndex].keys;

		for (uint32_t edgeIndex = 0;
			edgeIndex < 3;
			++edgeIndex)
		{
			const uint32_t nextIndex =
				(edgeIndex + 1) % 3;

			edges.push_back(
				makeSurfaceEdgeOccurrence(
					triangle.vertices[edgeIndex],
					triangle.vertices[nextIndex],
					triangleIndex,
					edgeIndex));
		}
	}

	std::sort(
		edges.begin(),
		edges.end(),
		isSurfaceEdgeLess);

	size_t index = 0;

	while (index < edges.size())
	{
		size_t nextIndex = index + 1;

		while (nextIndex < edges.size() &&
			isSurfaceEdgeEqual(
				edges[index],
				edges[nextIndex]))
		{
			++nextIndex;
		}

		const size_t useCount =
			nextIndex - index;

		if (useCount > 2)
		{
			mesh.adjacency.clear();
			return false;
		}

		if (useCount == 2)
		{
			const SurfaceEdgeOccurrence& a =
				edges[index];

			const SurfaceEdgeOccurrence& b =
				edges[index + 1];

			if (a.triangleIndex == b.triangleIndex)
			{
				mesh.adjacency.clear();
				return false;
			}

			mesh.adjacency[a.triangleIndex]
				.neighbors[a.edgeIndex] =
				b.triangleIndex;

			mesh.adjacency[b.triangleIndex]
				.neighbors[b.edgeIndex] =
				a.triangleIndex;
		}

		index = nextIndex;
	}

	return true;
}
