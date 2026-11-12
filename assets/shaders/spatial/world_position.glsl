
///////////////////////////////////////////////////////////////////////////////
// world_position.glsl
// ===================
//
// Provides chunk/local world-space coordinates for GPU shader operations.
//
// Chunk coordinates identify the integer spatial grid. Local positions
// retain floating-point precision independently of world distance.
//
// World positions are canonical when each local component is in the
// half-open interval [0, CHUNK_SIZE_METERS).
//
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* Spatial Constants
************************************************************/

const float CHUNK_SIZE_METERS = 16.0;

/***********************************************************
* Coordinate Types
************************************************************/

struct WorldPosition
{
	ivec3 chunk;
	vec3 localPosition;
};

/***********************************************************
* World Position Helpers
************************************************************/

WorldPosition makeWorldPosition(
	ivec3 chunkCoord,
	vec3 localPosition)
{
	ivec3 chunkOffset =
		ivec3(floor(
			localPosition /
			CHUNK_SIZE_METERS));

	WorldPosition result;

	result.chunk =
		chunkCoord + chunkOffset;

	result.localPosition =
		localPosition -
		vec3(chunkOffset) *
		CHUNK_SIZE_METERS;

	return result;
}
