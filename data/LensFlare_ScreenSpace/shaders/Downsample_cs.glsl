#include "shaders/def.glsl"

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

uniform int uSrcLevel;

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) {
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);

	vec4 ret = vec4(0.0);
	#if 1
	 // 3x3 Gaussian blur
		const float kernel[9] = {
			0.077847, 0.123317, 0.077847,
			0.123317, 0.195346, 0.123317,
			0.077847, 0.123317, 0.077847,
		};
		const vec2 scale = 2.0 / vec2(txSize); // scale can increase the blur radius 
		const vec2 offsets[9] = {
			vec2(-1.0, -1.0), vec2( 0.0, -1.0), vec2( 1.0, -1.0),
			vec2(-1.0,  0.0), vec2( 0.0,  0.0), vec2( 1.0,  0.0),
			vec2(-1.0,  1.0), vec2( 0.0,  1.0), vec2( 1.0,  1.0),
		};
		for (int i = 0; i < 9; ++i) {
			ret += textureLod(txSrc, uv + offsets[i] * scale, uSrcLevel) * kernel[i];
		}

	#else
		vec2 offset = 0.25 / vec2(txSize);
		ret += textureLod(txSrc, uv + vec2(-offset.x, -offset.x), uSrcLevel);
		ret += textureLod(txSrc, uv + vec2( offset.x, -offset.x), uSrcLevel);
		ret += textureLod(txSrc, uv + vec2( offset.x,  offset.x), uSrcLevel);
		ret += textureLod(txSrc, uv + vec2(-offset.x,  offset.x), uSrcLevel);
		ret *= 1.0 / 4.0;
	#endif
	
	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), ret);
}
