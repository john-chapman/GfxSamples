#include "shaders/def.glsl"
#include "shaders/Convolution.glsl"

#if (MODE != Mode_Separable)
	#error MODE != Mode_Separable, only 1d kernels are supported
#endif

#ifndef DIMENSION
	#error DIMENSION not defined
#endif

#define TEXELS_PER_THREAD   1  // \todo 
#define PACK_TEXEL_CACHE    1  // Pack texture fetches into uints.

uniform sampler2D txSrc;
uniform writeonly image2D txDst;

layout(std430) restrict readonly buffer bfWeights
{
	float uWeights[];
};
#define GetWeight(_i) (uWeights[_i])
#define GetOffset(_i) (_i - KERNEL_SIZE / 2)

#if PACK_TEXEL_CACHE
	#define CacheType uint
#else
	#define CacheType vec4
#endif
#define CACHE_WIDTH (gl_WorkGroupSize.x + (KERNEL_SIZE - 1))
shared CacheType s_cache[CACHE_WIDTH][gl_WorkGroupSize.y];

#define KERNEL_RADIUS (KERNEL_SIZE / 2)
#define CACHE_COORD (ivec2(gl_LocalInvocationID.xy) + ivec2(KERNEL_RADIUS, 0))
#if (DIMENSION == 0) 
	#define TEXEL_COORD (ivec2(gl_GlobalInvocationID.xy))
#else
	#define TEXEL_COORD (ivec2(gl_GlobalInvocationID.yx))
#endif

uint Pack(in vec4 _raw)
{
	uint ret = 0;
	ret = bitfieldInsert(ret, uint(_raw.x * 255.0), 24, 8);
	ret = bitfieldInsert(ret, uint(_raw.y * 255.0), 16, 8);
	ret = bitfieldInsert(ret, uint(_raw.z * 255.0),  8, 8);
	ret = bitfieldInsert(ret, uint(_raw.w * 255.0),  0, 8);
	return ret;
}

vec4 Unpack(in uint _packed)
{
	vec4 ret;
	ret.x = float(bitfieldExtract(_packed, 24, 8)) / 255.0;
	ret.y = float(bitfieldExtract(_packed, 16, 8)) / 255.0;
	ret.z = float(bitfieldExtract(_packed,  8, 8)) / 255.0;
	ret.w = float(bitfieldExtract(_packed,  0, 8)) / 255.0;
	return ret;
}

vec4 ReadCache(ivec2 _coord, int _offset)
{
	CacheType ret = s_cache[_coord.x + _offset][_coord.y];
	#if PACK_TEXEL_CACHE
		return Unpack(ret);
	#else
		return ret;
	#endif
}

void WriteCache(ivec2 _coord, vec4 _v)
{
	#if PACK_TEXEL_CACHE
		s_cache[_coord.x][_coord.y] = Pack(_v);
	#else
		s_cache[_coord.x][_coord.y] = _v;
	#endif
}

void main()
{
	ivec2 txSize = ivec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(TEXEL_COORD, txSize))) 
	{
		return;
	}

 // cache texel at the thread's location
	WriteCache(CACHE_COORD, texelFetch(txSrc, ivec2(TEXEL_COORD), 0)); 

 // cache border texels
	if (int(gl_LocalInvocationID.x) < KERNEL_RADIUS)
	{
		ivec2 texelCoord             = TEXEL_COORD;
		      texelCoord[DIMENSION]  = clamp(texelCoord[DIMENSION] - KERNEL_RADIUS, 0, txSize[DIMENSION] - 1);
		ivec2 cacheCoord             = ivec2(CACHE_COORD.x - KERNEL_RADIUS, CACHE_COORD.y);
		WriteCache(cacheCoord, texelFetch(txSrc, ivec2(texelCoord), 0));
	}
	if (int(gl_LocalInvocationID.x) >= (int(gl_WorkGroupSize.x) - KERNEL_RADIUS))
	{
		ivec2 texelCoord             = TEXEL_COORD;
		      texelCoord[DIMENSION]  = clamp(texelCoord[DIMENSION] + KERNEL_RADIUS, 0, txSize[DIMENSION] - 1);
		ivec2 cacheCoord             = ivec2(CACHE_COORD.x + KERNEL_RADIUS, CACHE_COORD.y);
		WriteCache(cacheCoord, texelFetch(txSrc, ivec2(texelCoord), 0));
	}

 // issue a memory barrier for the cache and synchronize all threads in the group before reading it
	memoryBarrierShared();
	barrier();

 // perform the convolution
	vec4 ret = vec4(0.0);
	for (int i = 0; i < KERNEL_SIZE; ++i)
	{
		ret += ReadCache(CACHE_COORD, i - KERNEL_RADIUS) * GetWeight(i);	
	}
	
	imageStore(txDst, ivec2(TEXEL_COORD), ret);
}
