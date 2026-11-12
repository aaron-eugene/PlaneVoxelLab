
///////////////////////////////////////////////////////////////////////////////
// sphere_density.glsl
// ===================
//
// Implements sphere density sampling using chunk/local world coordinates.
//
// Negative density is inside the sphere, positive density is outside,
// and zero is on its surface.
//
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* Sphere Parameters
************************************************************/

uniform ivec3 uSphereCenterChunk;
uniform vec3 uSphereCenterLocal;
uniform float uSphereRadius;

/***********************************************************
* Sphere Density Sampling
************************************************************/

float sampleDensityField(
	WorldPosition worldPosition)
{
	ivec3 chunkDelta =
		worldPosition.chunk -
		uSphereCenterChunk;

	vec3 offset =
		vec3(chunkDelta) *
		CHUNK_SIZE_METERS +
		worldPosition.localPosition -
		uSphereCenterLocal;

	return length(offset) -
		uSphereRadius;
}
