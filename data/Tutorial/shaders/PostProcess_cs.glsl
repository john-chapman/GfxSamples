#include "shaders/def.glsl"

layout(rgba8) uniform image2D txScene;

void main()
{
 // discard any redundant thread invocations
	vec2 txSize = vec2(imageSize(txScene).xy);
	if (any(greaterThanEqual(ivec2(gl_GlobalInvocationID.xy), ivec2(txSize)))) {
		return;
	}

	vec3 ret = imageLoad(txScene, ivec2(gl_GlobalInvocationID.xy)).rgb;
	float luminance = dot(ret, vec3(0.25, 0.5, 0.25));
	imageStore(txScene, ivec2(gl_GlobalInvocationID.xy), vec4(luminance));
}
