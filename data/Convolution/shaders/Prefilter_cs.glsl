#include "shaders/def.glsl"

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

uniform int uSrcLevel;

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) 
	{
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);

	vec4 ret = vec4(0.0);
	#if 0
	 // simple box filter
		ret = textureLod(txSrc, uv, uSrcLevel);
	#else
	 // 9 tap 5x5 Gaussian prefilter kernel  
		if (uSrcLevel == -1)
		{
			ret = textureLod(txSrc, uv, 0.0);
		}
		else
		{
			const vec2 kOffsets[] =
			{
				vec2(-1.223379, -1.223379), vec2(0.397811, -1.223379), vec2(2.000000, -1.223379), 
				vec2(-1.223379,  0.397811), vec2(0.397811,  0.397811), vec2(2.000000,  0.397811), 
				vec2(-1.223379,  2.000000), vec2(0.397811,  2.000000), vec2(2.500000,  2.500000), 
			};

			const float kWeights[] =
			{
				0.099162, 0.193587, 0.022151, 
				0.193587, 0.377927, 0.043243, 
				0.022151, 0.043243, 0.004948, 
			};

			vec2 texelSize = vec2(1.0) / vec2(textureSize(txSrc, uSrcLevel));
			for (uint i = 0; i < kOffsets.length(); ++i)
			{
				ret += textureLod(txSrc, uv + kOffsets[i] * texelSize, uSrcLevel) * kWeights[i];
			}
		}
	#endif
	
	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), ret);
}
