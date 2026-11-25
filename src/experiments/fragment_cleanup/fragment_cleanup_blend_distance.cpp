
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_blend_distance.cpp
// ==================================================================
//
// Propagates material-boundary distances through surface adjacency.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_blend_distance.h"

#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"
#include "experiments/fragment_cleanup/fragment_cleanup_cross_chunk.h"
#include "experiments/fragment_cleanup/fragment_cleanup_experiment.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <tuple>
#include <vector>

/***********************************************************
* Local Types
************************************************************/

namespace
{
	struct BlendDistanceQueueEntry
	{
		float distance = 0.0f;
		uint32_t triangleIndex = 0;

		bool operator>(
			const BlendDistanceQueueEntry& other) const
		{
			return distance > other.distance;
		}
	};
}

/***********************************************************
* Geometry Helpers
************************************************************/

static glm::vec3 getTriangleCentroid(
	const FragmentCleanupTopologyTriangle& triangle)
{
	return
		(triangle.positions[0] +
			triangle.positions[1] +
			triangle.positions[2]) / 3.0f;
}

static glm::vec3 getTriangleEdgeMidpoint(
	const FragmentCleanupTopologyTriangle& triangle,
	uint32_t edgeIndex)
{
	assert(edgeIndex < 3);

	return
		(triangle.positions[edgeIndex] +
			triangle.positions[(edgeIndex + 1) % 3]) * 0.5f;
}

static float getTriangleTransitionDistance(
	const FragmentCleanupMesh& mesh,
	uint32_t triangleIndex,
	uint32_t edgeIndex,
	uint32_t neighborIndex)
{
	const FragmentCleanupTopologyTriangle& triangle =
		mesh.topologyTriangles[triangleIndex];

	const FragmentCleanupTopologyTriangle& neighbor =
		mesh.topologyTriangles[neighborIndex];

	const glm::vec3 edgeMidpoint =
		getTriangleEdgeMidpoint(
			triangle,
			edgeIndex);

	return
		glm::length(
			getTriangleCentroid(triangle) - edgeMidpoint) +
		glm::length(
			getTriangleCentroid(neighbor) - edgeMidpoint);
}

/***********************************************************
* Traversal Helpers
************************************************************/

static bool canTraverseBlendTriangle(
	const FragmentCleanupTopologyTriangle& triangle,
	TerrainTile targetMaterial)
{
	return
		!triangle.hasMaterial ||
		triangle.material == targetMaterial;
}


/***********************************************************
* World Blend Traversal Types
************************************************************/

namespace
{
	struct WorldBlendQueueEntry
	{
		float distance = 0.0f;
		uint32_t meshIndex = 0;
		uint32_t triangleIndex = 0;

		bool operator>(
			const WorldBlendQueueEntry& other) const
		{
			return distance > other.distance;
		}
	};

	struct WorldBlendCrossNeighbor
	{
		FragmentCleanupCrossChunkEndpoint from = {};
		FragmentCleanupCrossChunkEndpoint to = {};
	};
}

/***********************************************************
* Cross-Chunk Lookup Helpers
************************************************************/

static bool isWorldBlendEndpointLess(
	const FragmentCleanupCrossChunkEndpoint& a,
	const FragmentCleanupCrossChunkEndpoint& b)
{
	return
		std::tie(a.meshIndex, a.triangleIndex, a.edgeIndex) <
		std::tie(b.meshIndex, b.triangleIndex, b.edgeIndex);
}

static std::vector<WorldBlendCrossNeighbor>
buildWorldBlendCrossNeighborLookup(
	const std::vector<FragmentCleanupCrossChunkConnection>& connections)
{
	std::vector<WorldBlendCrossNeighbor> lookup = {};

	lookup.reserve(connections.size() * 2);

	for (const FragmentCleanupCrossChunkConnection& connection :
		connections)
	{
		lookup.push_back({ connection.a, connection.b });
		lookup.push_back({ connection.b, connection.a });
	}

	std::sort(
		lookup.begin(),
		lookup.end(),
		[](const WorldBlendCrossNeighbor& a,
			const WorldBlendCrossNeighbor& b)
		{
			return isWorldBlendEndpointLess(a.from, b.from);
		});

#ifndef NDEBUG
	for (size_t index = 1; index < lookup.size(); ++index)
	{
		assert(isWorldBlendEndpointLess(
			lookup[index - 1].from,
			lookup[index].from));
	}
#endif

	return lookup;
}

static const WorldBlendCrossNeighbor* findWorldBlendCrossNeighbor(
	const std::vector<WorldBlendCrossNeighbor>& lookup,
	const FragmentCleanupCrossChunkEndpoint& endpoint)
{
	const auto iterator =
		std::lower_bound(
			lookup.begin(),
			lookup.end(),
			endpoint,
			[](const WorldBlendCrossNeighbor& entry,
				const FragmentCleanupCrossChunkEndpoint& key)
			{
				return isWorldBlendEndpointLess(entry.from, key);
			});

	if (iterator == lookup.end() ||
		isWorldBlendEndpointLess(endpoint, iterator->from))
	{
		return nullptr;
	}

	return &(*iterator);
}

/***********************************************************
* World Distance Helpers
************************************************************/

static float getWorldBlendTransitionDistance(
	const std::vector<FragmentCleanupRenderMesh>& meshes,
	uint32_t meshIndex,
	uint32_t triangleIndex,
	uint32_t edgeIndex,
	uint32_t neighborMeshIndex,
	uint32_t neighborTriangleIndex)
{
	const FragmentCleanupTopologyTriangle& triangle =
		meshes[meshIndex].cpuMesh.topologyTriangles[triangleIndex];

	const FragmentCleanupTopologyTriangle& neighbor =
		meshes[neighborMeshIndex].cpuMesh.topologyTriangles[
			neighborTriangleIndex];

	const glm::vec3 edgeMidpoint =
		getTriangleEdgeMidpoint(triangle, edgeIndex);

	const glm::vec3 neighborOffset =
		getChunkRelativePosition(
			meshes[neighborMeshIndex].coord,
			meshes[meshIndex].coord);

	const glm::vec3 neighborCentroid =
		getTriangleCentroid(neighbor) + neighborOffset;

	return
		glm::length(
			getTriangleCentroid(triangle) - edgeMidpoint) +
		glm::length(
			neighborCentroid - edgeMidpoint);
}

/***********************************************************
* Material Blend Distance Interface
************************************************************/

uint32_t calculateFragmentCleanupBlendDistances(
	std::vector<float>& distances,
	const FragmentCleanupMesh& mesh,
	TerrainTile targetMaterial,
	TerrainTile sourceMaterial,
	float maxDistanceMeters)
{
	const float infinity =
		std::numeric_limits<float>::infinity();

	distances.assign(
		mesh.topologyTriangles.size(),
		infinity);

	assert(
		mesh.adjacency.size() ==
		mesh.topologyTriangles.size());

	assert(targetMaterial != sourceMaterial);
	assert(maxDistanceMeters > 0.0f);

	using Queue = std::priority_queue<
		BlendDistanceQueueEntry,
		std::vector<BlendDistanceQueueEntry>,
		std::greater<BlendDistanceQueueEntry>>;

	Queue queue = {};

	uint32_t seededBoundaryCount = 0;

	//--------------------------------------------------
	// Initialize From Material Boundaries
	//--------------------------------------------------

	for (const FragmentCleanupMaterialBoundary& boundary :
		mesh.materialBoundaries)
	{
		const uint32_t triangleIndex =
			boundary.triangleIndex;

		const uint32_t neighborIndex =
			mesh.adjacency[triangleIndex]
			.neighbors[boundary.edgeIndex];

		assert(neighborIndex < mesh.topologyTriangles.size());

		const FragmentCleanupTopologyTriangle& triangle =
			mesh.topologyTriangles[triangleIndex];

		const FragmentCleanupTopologyTriangle& neighbor =
			mesh.topologyTriangles[neighborIndex];

		uint32_t targetIndex = 0;
		uint32_t targetEdgeIndex = 0;

		if (triangle.material == targetMaterial &&
			neighbor.material == sourceMaterial)
		{
			targetIndex = triangleIndex;
			targetEdgeIndex = boundary.edgeIndex;
		}
		else if (triangle.material == sourceMaterial &&
			neighbor.material == targetMaterial)
		{
			targetIndex = neighborIndex;

			const FragmentCleanupTriangleAdjacency& targetAdjacency =
				mesh.adjacency[targetIndex];

			bool foundEdge = false;

			for (uint32_t edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
			{
				if (targetAdjacency.neighbors[edgeIndex] ==
					triangleIndex)
				{
					targetEdgeIndex = edgeIndex;
					foundEdge = true;
					break;
				}
			}

			assert(foundEdge);

			if (!foundEdge)
			{
				continue;
			}
		}
		else
		{
			continue;
		}

		++seededBoundaryCount;

		const FragmentCleanupTopologyTriangle& target =
			mesh.topologyTriangles[targetIndex];

		const float distance =
			glm::length(
				getTriangleCentroid(target) -
				getTriangleEdgeMidpoint(
					target,
					targetEdgeIndex));

		if (distance < distances[targetIndex] &&
			distance <= maxDistanceMeters)
		{
			distances[targetIndex] = distance;

			queue.push(
				{ distance, targetIndex });
		}
	}

	//--------------------------------------------------
	// Propagate Through Surface Adjacency
	//--------------------------------------------------

	while (!queue.empty())
	{
		const BlendDistanceQueueEntry current =
			queue.top();

		queue.pop();

		if (current.distance !=
			distances[current.triangleIndex])
		{
			continue;
		}

		const FragmentCleanupTriangleAdjacency& adjacency =
			mesh.adjacency[current.triangleIndex];

		for (uint32_t edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
		{
			const uint32_t neighborIndex =
				adjacency.neighbors[edgeIndex];

			if (neighborIndex ==
				std::numeric_limits<uint32_t>::max())
			{
				continue;
			}

			const FragmentCleanupTopologyTriangle& neighbor =
				mesh.topologyTriangles[neighborIndex];

			if (!canTraverseBlendTriangle(
				neighbor,
				targetMaterial))
			{
				continue;
			}

			const float transitionDistance =
				getTriangleTransitionDistance(
					mesh,
					current.triangleIndex,
					edgeIndex,
					neighborIndex);

			const float candidateDistance =
				current.distance + transitionDistance;

			if (candidateDistance >= distances[neighborIndex] ||
				candidateDistance > maxDistanceMeters)
			{
				continue;
			}

			distances[neighborIndex] = candidateDistance;

			queue.push(
				{ candidateDistance, neighborIndex });
		}
	}

	return seededBoundaryCount;
}

/***********************************************************
* World Material Blend Distance Interface
************************************************************/

uint32_t calculateFragmentCleanupWorldBlendDistances(
	std::vector<std::vector<float>>& distances,
	std::vector<std::vector<uint8_t>>& boundarySeeds,
	const std::vector<FragmentCleanupRenderMesh>& meshes,
	const std::vector<FragmentCleanupCrossChunkConnection>& connections,
	TerrainTile targetMaterial,
	TerrainTile sourceMaterial,
	float maxDistanceMeters)
{
	assert(targetMaterial != sourceMaterial);
	assert(maxDistanceMeters > 0.0f);
	assert(meshes.size() <= UINT32_MAX);

	const float infinity =
		std::numeric_limits<float>::infinity();

	distances.clear();
	distances.resize(meshes.size());

	boundarySeeds.clear();
	boundarySeeds.resize(meshes.size());

	for (size_t meshIndex = 0;
		meshIndex < meshes.size();
		++meshIndex)
	{
		const FragmentCleanupMesh& mesh =
			meshes[meshIndex].cpuMesh;

		assert(mesh.topologyTriangles.size() <= UINT32_MAX);
		assert(mesh.adjacency.size() ==
			mesh.topologyTriangles.size());

		distances[meshIndex].assign(
			mesh.topologyTriangles.size(),
			infinity);

		boundarySeeds[meshIndex].assign(
			mesh.topologyTriangles.size(),
			0);
	}

	const std::vector<WorldBlendCrossNeighbor> crossNeighbors =
		buildWorldBlendCrossNeighborLookup(connections);

	using Queue = std::priority_queue<
		WorldBlendQueueEntry,
		std::vector<WorldBlendQueueEntry>,
		std::greater<WorldBlendQueueEntry>>;

	Queue queue = {};

	uint32_t seededBoundaryCount = 0;

	//--------------------------------------------------
	// Boundary Seeding
	//--------------------------------------------------

	const auto seedBoundary =
		[&](uint32_t meshIndex,
			uint32_t triangleIndex,
			uint32_t edgeIndex)
		{
			const FragmentCleanupTopologyTriangle& triangle =
				meshes[meshIndex].cpuMesh.topologyTriangles[
					triangleIndex];

			const float distance =
				glm::length(
					getTriangleCentroid(triangle) -
					getTriangleEdgeMidpoint(triangle, edgeIndex));

			++seededBoundaryCount;

			if (distance <= maxDistanceMeters)
			{
				boundarySeeds[meshIndex][triangleIndex] = 1;
			}

			if (distance > maxDistanceMeters ||
				distance >= distances[meshIndex][triangleIndex])
			{
				return;
			}

			distances[meshIndex][triangleIndex] = distance;

			queue.push(
				{ distance, meshIndex, triangleIndex });
		};

	//--------------------------------------------------
	// Seed Chunk-Local Material Boundaries
	//--------------------------------------------------

	for (uint32_t meshIndex = 0;
		meshIndex < meshes.size();
		++meshIndex)
	{
		const FragmentCleanupMesh& mesh =
			meshes[meshIndex].cpuMesh;

		for (const FragmentCleanupMaterialBoundary& boundary :
			mesh.materialBoundaries)
		{
			const uint32_t triangleIndex =
				boundary.triangleIndex;

			const uint32_t neighborIndex =
				mesh.adjacency[triangleIndex]
				.neighbors[boundary.edgeIndex];

			assert(neighborIndex < mesh.topologyTriangles.size());

			const FragmentCleanupTopologyTriangle& triangle =
				mesh.topologyTriangles[triangleIndex];

			const FragmentCleanupTopologyTriangle& neighbor =
				mesh.topologyTriangles[neighborIndex];

			if (triangle.material == targetMaterial &&
				neighbor.material == sourceMaterial)
			{
				seedBoundary(
					meshIndex,
					triangleIndex,
					boundary.edgeIndex);
			}
			else if (triangle.material == sourceMaterial &&
				neighbor.material == targetMaterial)
			{
				for (uint32_t edgeIndex = 0;
					edgeIndex < 3;
					++edgeIndex)
				{
					if (mesh.adjacency[neighborIndex]
						.neighbors[edgeIndex] == triangleIndex)
					{
						seedBoundary(
							meshIndex,
							neighborIndex,
							edgeIndex);

						break;
					}
				}
			}
		}
	}

	//--------------------------------------------------
	// Seed Cross-Chunk Material Boundaries
	//--------------------------------------------------

	for (const FragmentCleanupCrossChunkConnection& connection :
		connections)
	{
		const FragmentCleanupCrossChunkEndpoint& a =
			connection.a;

		const FragmentCleanupCrossChunkEndpoint& b =
			connection.b;

		assert(a.meshIndex < meshes.size());
		assert(b.meshIndex < meshes.size());

		const FragmentCleanupTopologyTriangle& triangleA =
			meshes[a.meshIndex].cpuMesh.topologyTriangles[
				a.triangleIndex];

		const FragmentCleanupTopologyTriangle& triangleB =
			meshes[b.meshIndex].cpuMesh.topologyTriangles[
				b.triangleIndex];

		if (!triangleA.hasMaterial || !triangleB.hasMaterial)
		{
			continue;
		}

		if (triangleA.material == targetMaterial &&
			triangleB.material == sourceMaterial)
		{
			seedBoundary(
				a.meshIndex,
				a.triangleIndex,
				a.edgeIndex);
		}
		else if (triangleB.material == targetMaterial &&
			triangleA.material == sourceMaterial)
		{
			seedBoundary(
				b.meshIndex,
				b.triangleIndex,
				b.edgeIndex);
		}
	}

	//--------------------------------------------------
	// Propagate Through Local and Cross-Chunk Edges
	//--------------------------------------------------

	while (!queue.empty())
	{
		const WorldBlendQueueEntry current =
			queue.top();

		queue.pop();

		if (current.distance !=
			distances[current.meshIndex][current.triangleIndex])
		{
			continue;
		}

		const FragmentCleanupMesh& mesh =
			meshes[current.meshIndex].cpuMesh;

		const FragmentCleanupTriangleAdjacency& adjacency =
			mesh.adjacency[current.triangleIndex];

		for (uint32_t edgeIndex = 0;
			edgeIndex < 3;
			++edgeIndex)
		{
			uint32_t neighborMeshIndex =
				current.meshIndex;

			uint32_t neighborTriangleIndex =
				adjacency.neighbors[edgeIndex];

			if (neighborTriangleIndex == UINT32_MAX)
			{
				const FragmentCleanupCrossChunkEndpoint endpoint =
				{
					current.meshIndex,
					current.triangleIndex,
					edgeIndex
				};

				const WorldBlendCrossNeighbor* crossNeighbor =
					findWorldBlendCrossNeighbor(
						crossNeighbors,
						endpoint);

				if (crossNeighbor == nullptr)
				{
					continue;
				}

				neighborMeshIndex =
					crossNeighbor->to.meshIndex;

				neighborTriangleIndex =
					crossNeighbor->to.triangleIndex;
			}

			const FragmentCleanupTopologyTriangle& neighbor =
				meshes[neighborMeshIndex]
				.cpuMesh.topologyTriangles[neighborTriangleIndex];

			if (!canTraverseBlendTriangle(
				neighbor,
				targetMaterial))
			{
				continue;
			}

			const float transitionDistance =
				getWorldBlendTransitionDistance(
					meshes,
					current.meshIndex,
					current.triangleIndex,
					edgeIndex,
					neighborMeshIndex,
					neighborTriangleIndex);

			const float candidateDistance =
				current.distance + transitionDistance;

			if (candidateDistance > maxDistanceMeters ||
				candidateDistance >=
				distances[neighborMeshIndex][neighborTriangleIndex])
			{
				continue;
			}

			distances[neighborMeshIndex][neighborTriangleIndex] =
				candidateDistance;

			queue.push(
				{
					candidateDistance,
					neighborMeshIndex,
					neighborTriangleIndex
				});
		}
	}

	return seededBoundaryCount;
}
