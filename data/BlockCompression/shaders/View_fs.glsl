#include "shaders/def.glsl"
#include "shaders/Sampling.glsl"

noperspective in vec2 vUv;

uniform vec2      uUvScale;
uniform vec2      uUvBias;
uniform sampler2D txSrc;
uniform sampler2D txCmp;

layout(std430) restrict readonly buffer _bfSrc
{
	uvec2 bfSrc[];
};

#define Mode_None       0
#define Mode_Source     1
#define Mode_Error      2
#define Mode_BlockEP0   3
#define Mode_BlockEP1   4
uniform int uMode;

layout(location=0) out vec4 fResult;

vec3 UnpackRGB565(in uint _565)
{
	vec3 ret;
	ret.r = float(bitfieldExtract(_565, 11, 5)) / 31.0;
	ret.g = float(bitfieldExtract(_565, 5,  6)) / 63.0;
	ret.b = float(bitfieldExtract(_565, 0,  5)) / 31.0;
	return ret;
}

uint PackRGB565(in vec3 _rgb)
{
	uint ret = 0;
	ret = bitfieldInsert(ret, uint(_rgb.r * 31.0), 11, 5);
	ret = bitfieldInsert(ret, uint(_rgb.g * 63.0), 5,  6);
	ret = bitfieldInsert(ret, uint(_rgb.b * 31.0), 0,  5);
	return ret;
}

void main() 
{
	vec2 uv = vec2(vUv.x, 1.0 - vUv.y); // textures are loaded flipped
	uv = uUvScale * uv + uUvBias;
	uvec2 tsize = textureSize(txSrc, 0).xy;
	uvec2 iuv = uvec2(uv * vec2(tsize));
	uint  blockIndex = (iuv.y / 4) * (tsize.x / 4) + (iuv.x / 4);

	vec3 src = textureLod(txSrc, uv, 0.0).rgb;
	vec3 cmp = textureLod(txCmp, uv, 0.0).rgb;

	vec3 ret = vec3(0.0);
	switch (uMode) 
	{
		default:
		case Mode_None:
		{
			ret = src;
			break;
		}
		case Mode_Source:
		{
			ret = cmp;
			break;
		}
		case Mode_Error:
		{
			ret = abs(src - cmp) * 10.0;
			ret = vec3(max3(ret.x, ret.y, ret.z));
			//ret = vec3(length2(ret));
			break;
		}
		case Mode_BlockEP0:
		{
			uint block = bitfieldExtract(bfSrc[blockIndex].x, 16, 16);
			ret = UnpackRGB565(block);
			break;
		}
		case Mode_BlockEP1:
		{
			uint block = bitfieldExtract(bfSrc[blockIndex].x, 0, 16);
			ret = UnpackRGB565(block);
			break;
		}
	};

 // dbg color alpha 0 blocks as pink
	uint ep0 = bitfieldExtract(bfSrc[blockIndex].x, 0, 16);
	uint ep1 = bitfieldExtract(bfSrc[blockIndex].x, 16, 16);
	if (ep1 > ep0)
	{
		ret = vec3(1.0, 0.0, 1.0);
	}

	fResult = vec4(ret, 1.0);
}
