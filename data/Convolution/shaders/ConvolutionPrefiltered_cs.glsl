#include "shaders/def.glsl"
#include "shaders/Noise.glsl"
#include "shaders/Rand.glsl"
#include "shaders/Sampling.glsl"

#define ROTATE_KERNEL 1  // Randomly rotate the sampling kernel per pixel

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

uniform float uRadius;
uniform float uLod;
uniform int   uSampleCount;

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) 
	{
		return;
	}
	vec2 iuv = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
	vec2 texelSize = 1.0 / vec2(txSize);
	vec2 uv  = iuv * texelSize;

	#if ROTATE_KERNEL
		float theta =
			#if 1
				Noise_InterleavedGradient(iuv);
			#else
				Bayer_4x4(gl_GlobalInvocationID.xy);
			#endif
		theta = theta * k2Pi;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		mat2 rotation = mat2(
			 cosTheta, sinTheta,
			-sinTheta, cosTheta
			);
	#endif 

	vec4 ret = vec4(0.0);
	float rn = 1.0 / float(uSampleCount);
	for (uint i = 0; i < uSampleCount; ++i)
	{
		vec2 offset = Sampling_Hammersley2d(i, rn) * 2.0 - 1.0;
		#if ROTATE_KERNEL
			offset = rotation * offset;
		#endif
		ret += textureLod(txSrc, uv + offset * uRadius * texelSize, uLod);
	}
	ret = ret * rn;

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), ret);
}
