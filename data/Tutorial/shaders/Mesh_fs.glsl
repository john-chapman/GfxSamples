#include "shaders/def.glsl"

smooth in vec2 vUv;
smooth in vec3 vNormalW;
smooth in vec3 vPositionW;	

layout(location=0) out vec3 fResult;

uniform sampler2D txDiffuse;

void main() 
{
 // defines set from the code
#if DEFINE_ONE
 // do something conditional
#endif

	fResult = texture(txDiffuse, vUv).rgb * max(0.5, saturate(dot(vNormalW, vec3(1.0))));
}
