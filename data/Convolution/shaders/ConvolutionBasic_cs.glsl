#include "shaders/def.glsl"
#include "shaders/Convolution.glsl"

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

#if (MODE == Mode_2d || MODE == Mode_2dBilinear)
	#define OffsetType vec2
	#define GetOffset(_iuv, _i) (_iuv + uOffsets[_i])
#else
	#define OffsetType float
	uniform vec2 uDirection;
	#define GetOffset(_iuv, _i) (_iuv + vec2(uOffsets[_i]) * uDirection)
#endif
#define GetWeight(_i) (uWeights[_i])

layout(std430) restrict readonly buffer bfWeights
{
	float uWeights[];
};
layout(std430) restrict readonly buffer bfOffsets
{
	OffsetType uOffsets[];
};

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) 
	{
		return;
	}
	vec2 iuv = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
	vec2 texelSize = 1.0 / vec2(txSize);

	vec4 ret = vec4(0.0);
	for (int i = 0; i < KERNEL_SIZE; ++i) 
	{
		ret += textureLod(txSrc, GetOffset(iuv, i) * texelSize, 0.0) * GetWeight(i);
	}

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), ret);
}
