
///////////////////////////////////////////////////////////////////////////////
// experiments/fragment_cleanup/fragment_cleanup_material_boundaries.cpp
// =======================================================================
//
// Finds shared edges between triangles with different materials.
//
///////////////////////////////////////////////////////////////////////////////

#include "experiments/fragment_cleanup/fragment_cleanup_material_boundaries.h"

#include "experiments/fragment_cleanup/fragment_cleanup_builder.h"

#include <cassert>
#include <cstdint>

/***********************************************************
* Material Boundary Interface
************************************************************/

void buildFragmentCleanupMaterialBoundaries(
	FragmentCleanupMesh& mesh)
{
	mesh.materialBoundaries.clear();

	assert(
		mesh.adjacency.size() ==
		mesh.topologyTriangles.size());

	const uint32_t invalidIndex = UINT32_MAX;

	for (uint32_t triangleIndex = 0;
		triangleIndex < mesh.topologyTriangles.size();
		++triangleIndex)
	{
		const FragmentCleanupTopologyTriangle& triangle =
			mesh.topologyTriangles[triangleIndex];

		if (!triangle.hasMaterial)
		{
			continue;
		}

		const FragmentCleanupTriangleAdjacency& adjacency =
			mesh.adjacency[triangleIndex];

		for (uint32_t edgeIndex = 0;
			edgeIndex < 3;
			++edgeIndex)
		{
			const uint32_t neighborIndex =
				adjacency.neighbors[edgeIndex];

			if (neighborIndex == invalidIndex)
			{
				continue;
			}

			// Process each shared edge only once.
			if (triangleIndex >= neighborIndex)
			{
				continue;
			}

			const FragmentCleanupTopologyTriangle& neighbor =
				mesh.topologyTriangles[neighborIndex];

			// Rejected triangles remain neutral for now.
			if (!neighbor.hasMaterial)
			{
				continue;
			}

			if (triangle.material == neighbor.material)
			{
				continue;
			}

			FragmentCleanupMaterialBoundary boundary = {};

			boundary.triangleIndex = triangleIndex;
			boundary.edgeIndex = edgeIndex;

			mesh.materialBoundaries.push_back(boundary);
		}
	}
}
