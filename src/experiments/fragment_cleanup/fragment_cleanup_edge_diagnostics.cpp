
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_edge_diagnostics.cpp
// =====================================================================
//
// Analyzes marching tetrahedra surface-edge identities.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_edge_diagnostics.h"

#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"
#include "spatial/spatial_constants.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

/***********************************************************
* Local Types
************************************************************/

namespace
{
	struct SurfaceEdgeKey
	{
		SurfaceIntersectionKey vertexA = {};
		SurfaceIntersectionKey vertexB = {};
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

static bool isSampleCoordEqual(
	const SampleCoord& a,
	const SampleCoord& b)
{
	return
		a.x == b.x &&
		a.y == b.y &&
		a.z == b.z;
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

static bool isIntersectionKeyEqual(
	const SurfaceIntersectionKey& a,
	const SurfaceIntersectionKey& b)
{
	return
		isSampleCoordEqual(a.sampleA, b.sampleA) &&
		isSampleCoordEqual(a.sampleB, b.sampleB);
}

static bool isSurfaceEdgeLess(
	const SurfaceEdgeKey& a,
	const SurfaceEdgeKey& b)
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
	const SurfaceEdgeKey& a,
	const SurfaceEdgeKey& b)
{
	return
		isIntersectionKeyEqual(a.vertexA, b.vertexA) &&
		isIntersectionKeyEqual(a.vertexB, b.vertexB);
}

/***********************************************************
* Surface Edge Helpers
************************************************************/

static SurfaceEdgeKey makeSurfaceEdgeKey(
	const SurfaceIntersectionKey& vertexA,
	const SurfaceIntersectionKey& vertexB)
{
	SurfaceEdgeKey key = {};

	if (isIntersectionKeyLess(vertexB, vertexA))
	{
		key.vertexA = vertexB;
		key.vertexB = vertexA;
	}
	else
	{
		key.vertexA = vertexA;
		key.vertexB = vertexB;
	}

	return key;
}

/***********************************************************
* Chunk Boundary Helpers
************************************************************/

static bool isIntersectionOnBoundaryPlane(
	const SurfaceIntersectionKey& key,
	uint32_t axis,
	int32_t plane)
{
	const SampleCoord& a = key.sampleA;
	const SampleCoord& b = key.sampleB;

	switch (axis)
	{
	case 0:
		return a.x == plane && b.x == plane;

	case 1:
		return a.y == plane && b.y == plane;

	case 2:
		return a.z == plane && b.z == plane;

	default:
		assert(false);
		return false;
	}
}

static bool isSurfaceEdgeOnChunkBoundary(
	const SurfaceEdgeKey& edge)
{
	const int32_t maxSample =
		static_cast<int32_t>(
			CHUNK_SIZE_METERS / VOXEL_SIZE_METERS);

	for (uint32_t axis = 0; axis < 3; ++axis)
	{
		if (isIntersectionOnBoundaryPlane(
			edge.vertexA, axis, 0) &&
			isIntersectionOnBoundaryPlane(
				edge.vertexB, axis, 0))
		{
			return true;
		}

		if (isIntersectionOnBoundaryPlane(
			edge.vertexA, axis, maxSample) &&
			isIntersectionOnBoundaryPlane(
				edge.vertexB, axis, maxSample))
		{
			return true;
		}
	}

	return false;
}

/***********************************************************
* Surface Edge Diagnostic Interface
************************************************************/

FragmentCleanupEdgeStats analyzeFragmentCleanupEdges(
	const FragmentCleanupMesh& mesh)
{
	using Clock = std::chrono::steady_clock;

	const auto startTime = Clock::now();

	FragmentCleanupEdgeStats stats = {};

	assert(mesh.indices.size() % 3 == 0);
	assert(
		mesh.triangleKeys.size() ==
		mesh.indices.size() / 3);

	stats.triangleCount = mesh.topologyTriangles.size();

	stats.renderedTriangleCount =
		mesh.triangleKeys.size();

	stats.rejectedTriangleCount =
		stats.triangleCount -
		stats.renderedTriangleCount;

	std::vector<SurfaceEdgeKey> edges = {};

	edges.reserve(
		mesh.topologyTriangles.size() * 3);

	for (const FragmentCleanupTopologyTriangle& topologyTriangle :
		mesh.topologyTriangles)
	{
		const SurfaceTriangleKeys& triangle =
			topologyTriangle.keys;

		for (uint32_t edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
		{
			const uint32_t nextIndex =
				(edgeIndex + 1) % 3;

			const SurfaceIntersectionKey& vertexA =
				triangle.vertices[edgeIndex];

			const SurfaceIntersectionKey& vertexB =
				triangle.vertices[nextIndex];

			assert(!isIntersectionKeyEqual(vertexA, vertexB));

			edges.push_back(
				makeSurfaceEdgeKey(
					vertexA,
					vertexB));
		}
	}

	stats.edgeOccurrences = edges.size();

	std::sort(
		edges.begin(),
		edges.end(),
		isSurfaceEdgeLess);

	size_t index = 0;

	while (index < edges.size())
	{
		size_t nextIndex = index + 1;

		while (nextIndex < edges.size() &&
			isSurfaceEdgeEqual(edges[index], edges[nextIndex]))
		{
			++nextIndex;
		}

		const uint32_t useCount =
			static_cast<uint32_t>(nextIndex - index);

		++stats.uniqueEdges;

		if (useCount == 1)
		{
			++stats.singleUseEdges;

			if (isSurfaceEdgeOnChunkBoundary(edges[index]))
			{
				++stats.boundarySingleUseEdges;
			}
			else
			{
				++stats.interiorSingleUseEdges;

				if (stats.interiorSingleUseEdges <= 10)
				{
					const SurfaceEdgeKey& edge = edges[index];

					std::printf(
						"Interior unmatched edge:\n"
						"  A: (%d,%d,%d) -> (%d,%d,%d)\n"
						"  B: (%d,%d,%d) -> (%d,%d,%d)\n",
						edge.vertexA.sampleA.x,
						edge.vertexA.sampleA.y,
						edge.vertexA.sampleA.z,
						edge.vertexA.sampleB.x,
						edge.vertexA.sampleB.y,
						edge.vertexA.sampleB.z,
						edge.vertexB.sampleA.x,
						edge.vertexB.sampleA.y,
						edge.vertexB.sampleA.z,
						edge.vertexB.sampleB.x,
						edge.vertexB.sampleB.y,
						edge.vertexB.sampleB.z);
				}
			}
		}
		else if (useCount == 2)
		{
			++stats.doubleUseEdges;
		}

		else
		{
			++stats.multiUseEdges;
		}

		stats.maximumEdgeUse =
			std::max(
				stats.maximumEdgeUse,
				useCount);

		index = nextIndex;
	}

	const auto endTime = Clock::now();

	stats.elapsedMilliseconds =
		std::chrono::duration<double, std::milli>(
			endTime - startTime).count();

	return stats;
}
