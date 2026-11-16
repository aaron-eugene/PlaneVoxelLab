///////////////////////////////////////////////////////////////////////////////
// math/fractal_noise.glsl
// =======================
//
// Evaluates multi-octave 2D value noise using CPU-prepared octave parameters.
// Requires world_metric_coordinate.glsl, noise_coordinates.glsl, and
// value_noise.glsl. The octave buffer uses std430 layout at binding 2.
//
///////////////////////////////////////////////////////////////////////////////

/***********************************************************
* Prepared Noise Parameters
************************************************************/

struct GpuNoiseOctave
{
	int numerator;
	int denominator;
	float amplitude;
	float reserved;
};

layout(std430, binding = 2) readonly buffer NoiseOctaveBuffer
{
	GpuNoiseOctave uNoiseOctaves[];
};

uniform uint uNoiseOctaveCount;
uniform uint uNoiseSeed;
uniform float uNoiseTotalAmplitude;

/***********************************************************
* Fractal Value Noise
************************************************************/

bool trySampleGpuFractalValueNoise2d(
	WorldMetricCoordinate worldX,
	WorldMetricCoordinate worldZ,
	out float value)
{
	value = 0.0;

	if (uNoiseOctaveCount == 0u ||
		uNoiseOctaveCount > 64u ||
		!(uNoiseTotalAmplitude > 0.0))
	{
		return false;
	}

	float total = 0.0;

	for (uint octave = 0u;
		octave < uNoiseOctaveCount;
		++octave)
	{
		GpuNoiseOctave parameters = uNoiseOctaves[octave];

		NoiseCoordinate64 x;
		NoiseCoordinate64 z;

		if (!tryGetScaledNoiseCoordinate64(
			worldX,
			parameters.numerator,
			parameters.denominator,
			x) ||
			!tryGetScaledNoiseCoordinate64(
				worldZ,
				parameters.numerator,
				parameters.denominator,
				z))
		{
			return false;
		}

		uint octaveSeed = uNoiseSeed + octave * 1013u;

		total +=
			sampleGpuValueNoise2d(x, z, octaveSeed) *
			parameters.amplitude;
	}

	value = total / uNoiseTotalAmplitude;

	return true;
}
