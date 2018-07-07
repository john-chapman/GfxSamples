#include "shaders/def.glsl"

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

uniform int  uRadiusPixels;
uniform vec2 uDirection;

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) {
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);

	int sampleCount = clamp(int(uRadiusPixels / 2), 4, int(64));
		
	float kSigma    = float(sampleCount) / 3.0; // low divisor = more blur but clamps the weight function above zero
	float kSigma2   = kSigma * kSigma;
		
 // set up incremental counter
	vec3 gaussInc;
	gaussInc.x = 1.0 / (sqrt(k2Pi) * kSigma);
	gaussInc.y = exp(-0.5 / kSigma2);
	gaussInc.z = gaussInc.y * gaussInc.y;
	
 // accumulate results
	vec4 ret = texture(txSrc, uv) * gaussInc.x;
	for (int i = 1; i < sampleCount; ++i) {
		gaussInc.xy *= gaussInc.yz;
		vec2 offset = float(i) * uDirection * (1.0 / vec2(txSize));
		ret += textureLod(txSrc, uv - offset, 0.0) * gaussInc.x;
		ret += textureLod(txSrc, uv + offset, 0.0) * gaussInc.x;
	}
	
	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), ret);
}
