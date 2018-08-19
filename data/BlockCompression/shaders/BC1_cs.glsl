#include "shaders/def.glsl"

#define USE_SHARED_MEMORY  1  // Load src block into shared memory (else call texelFetch() for each sample).
#define ENDPOINTS_PCA      0  // Use principle component analysis to find the block endpoints (else use the color space extents).

uniform sampler2D txSrc;

layout(std430) restrict writeonly buffer _bfDst
{
	uvec2 bfDst[];
};

#if USE_SHARED_MEMORY
	shared vec3 s_srcTexels[16];
	#define SRC_TEXEL(_i) s_srcTexels[_i]
#else
	#define SRC_TEXEL(_i) texelFetch(txSrc, ivec2(BLOCK_COORD) + ivec2(_i % 4, _i / 4), 0).rgb
#endif

shared uint s_indices[16];

uint PackRGB565(in vec3 _rgb)
{
	uint ret = 0;
	ret = bitfieldInsert(ret, uint(_rgb.r * 31.0), 11, 5);
	ret = bitfieldInsert(ret, uint(_rgb.g * 63.0), 5,  6);
	ret = bitfieldInsert(ret, uint(_rgb.b * 31.0), 0,  5);
	return ret;
}
vec3 UnpackRGB565(in uint _565)
{
	vec3 ret;
	ret.r = float(bitfieldExtract(_565, 11, 5)) / 31.0;
	ret.g = float(bitfieldExtract(_565, 5,  6)) / 63.0;
	ret.b = float(bitfieldExtract(_565, 0,  5)) / 31.0;
	return ret;
}

float Covariance(in int _x, in int _y, in vec3 _avg)
{
	float ret = 0.0;
	for (int i = 0; i < 16; ++i) {
		ret += (SRC_TEXEL(i)[_x] - _avg[_x]) * (SRC_TEXEL(i)[_y] - _avg[_y]);
	}
	ret *= 1.0/16.0;
	return ret;
} 

void main()
{
	#define TEXEL_INDEX (gl_LocalInvocationIndex)
	#define TEXEL_COORD (gl_LocalInvocationID.xy)
	#define BLOCK_INDEX (gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x)
	#define BLOCK_COORD (gl_WorkGroupID.xy * 4)

	#if USE_SHARED_MEMORY
	{
	 // load src block into shared memory (1 fetch per thread)
		ivec2 iuv = ivec2(BLOCK_COORD + TEXEL_COORD);
		s_srcTexels[TEXEL_INDEX] = texelFetch(txSrc, iuv, 0).rgb;
		groupMemoryBarrier();
	}
	#endif
	#if 0
	{
	 // copy src block into registers
		vec3 srcTexels[16];
		for (int i = 0; i < 16; ++i) {
			srcTexels[i] = SRC_TEXEL(i);
		}
		#undef SRC_TEXEL
		#define SRC_TEXEL(i) srcTexels[i]
	}
	#endif

	uvec2 block = uvec2(0);

 // find endpoints
	vec3 ep0 = vec3(1.0);
	vec3 ep1 = vec3(0.0);
	#if ENDPOINTS_PCA
	{
	 	ep0 = ep1 = SRC_TEXEL(0);
		vec3 avg = ep0;
		for (int i = 1; i < 16; ++i) {
			ep0  = min(ep0, SRC_TEXEL(i));
			ep1  = max(ep1, SRC_TEXEL(i));
			avg += SRC_TEXEL(i);
		}
		avg *= 1.0/16.0;
		//vec3 avg = (ep0 + ep1) * 0.5;

		float cRR = Covariance(0, 0, avg);
		float cRG = Covariance(0, 1, avg);
		float cRB = Covariance(0, 2, avg);
		float cGG = Covariance(1, 1, avg);
		float cGB = Covariance(1, 2, avg);
		float cBB = Covariance(2, 2, avg);
		mat3  C = mat3(
			cRR, cRG, cRB,
			cRG, cGG, cGB,
			cRB, cGB, cBB
			);

		// find endpoints
		vec3 vf = ep1 - ep0;
		for (int i = 0; i < 16; ++i) { // \TODO this doesn't seem to affect the quality much?
			float x = dot(vf, C[0]);
			float y = dot(vf, C[1]);
			float z = dot(vf, C[2]);
			vf = vec3(x, y, z);
		}
		float vflen = length2(vf);
		if (vflen > 1e-7) {
			vf /= sqrt(vflen);
		}

		float mind, maxd;
		mind = maxd = dot(vf, SRC_TEXEL(0));
		for (int i = 1; i < 16; ++i) {
			float d = dot(vf, SRC_TEXEL(i));
			if (d < mind) {
				ep0 = SRC_TEXEL(i);
				mind = d;
			}
			if (d > maxd) {
				ep1 = SRC_TEXEL(i);
				maxd = d;
			}
		}
	}
	#else
	{
	 // color space extents
		ep0 = ep1 = SRC_TEXEL(0);
		for (int i = 1; i < 16; ++i) {
			ep0 = min(ep0, SRC_TEXEL(i));
			ep1 = max(ep1, SRC_TEXEL(i));
		}
	}
	#endif

 // export endpoints
	uint ep0i = PackRGB565(ep0);	
	uint ep1i = PackRGB565(ep1);
	if (ENDPOINTS_PCA == 1 && ep0i > ep1i) { // only need to swap if using PCA
		vec3 tmp = ep1;
		ep1 = ep0;
		ep0 = tmp;
	}
	block[0] = bitfieldInsert(block[0], ep1i, 0,  16);
	block[0] = bitfieldInsert(block[0], ep0i, 16, 16);

 // early-out solid color blocks
 	if (ep0i == ep1i) {
		if (TEXEL_INDEX == 0) {
			bfDst[BLOCK_INDEX] = block;
		}
		return;
	}

 // find indices
	{
	 // pack/unpack the endpoint values = quantize endpoints to minimize final error
		ep0 = UnpackRGB565(PackRGB565(ep0));
		ep1 = UnpackRGB565(PackRGB565(ep1));
		
	 // project onto (ep1 - ep0)
		vec3 d = ep1 - ep0;
		float dlen = length(d);
		d /= dlen; // \todo fails for solid color blocks (dlen is 0)
	 	vec3 src = SRC_TEXEL(TEXEL_INDEX);
		float idx = dot(src - ep0, d) / dlen;

	 // round to nearest index
		idx = round(saturate(idx) * 3.0);
		const uvec4 idxMap = uvec4(1, 3, 2, 0);
		s_indices[TEXEL_INDEX] = idxMap[uint(idx)];
	}
	groupMemoryBarrier();

 // encode block
	if (TEXEL_INDEX == 0) {
	 // pack palette indices
		for (int i = 0; i < 16; ++i) {
			block[1] = bitfieldInsert(block[1], s_indices[i], i * 2, 2);
		}	
	 // write block data
		bfDst[BLOCK_INDEX] = block;
	}
	
}