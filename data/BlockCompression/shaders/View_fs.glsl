#include "shaders/def.glsl"

noperspective in vec2 vUv;

uniform vec2      uUvScale;
uniform vec2      uUvBias;
uniform sampler2D txSrc;
uniform sampler2D txCmp;

#define Mode_None       0
#define Mode_Difference 1
uniform int uMode;

layout(location=0) out vec4 fResult;

void main() 
{
	vec2 uv = vec2(vUv.x, 1.0 - vUv.y); // textures are loaded flipped
	uv = uUvScale * uv + uUvBias;

	vec3 src = textureLod(txSrc, uv, 0.0).rgb;
	vec3 cmp = textureLod(txCmp, uv, 0.0).rgb;

	vec3 ret = vec3(0.0);
	switch (uMode) {
		default:
		case Mode_None:
			ret = src;
			break;
		case Mode_Difference:
			ret = abs(src - cmp) * 10.0;
			break;
	};

	fResult = vec4(ret, 1.0);
}
