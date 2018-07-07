#include "shaders/def.glsl"

#define USE_SHARED_MEMORY  1  // Load src block into shared memory (else call texelFetch() for each sample).
#define ENDPOINTS_PCA      1  // Use principle component analysis to find the block endpoints (else use the color space extents).
#define INDICES_EUCLIDEAN  1  // Use euclidean distance to find the indices per pixel (else project onto the line segment).

uniform sampler2D txSrc;

layout(std430) restrict writeonly buffer _bfDst
{
	uvec2 bfDst[];
};

#if USE_SHARED_MEMORY
	shared vec3 s_srcTexels[16];
	#define SRC_TEXEL(_i) s_srcTexels[_i]
#else
	#define SRC_TEXEL(_i)  texelFetch(txSrc, ivec2(gl_WorkGroupID.xy * 4) + ivec2(_i % 4, _i / 4), 0).rgb
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
	#define THREAD_IDX gl_LocalInvocationIndex

	#if USE_SHARED_MEMORY
	{
	 // load src block into shared memory (1 fetch per thread)
		ivec2 iuv = ivec2(gl_WorkGroupID.xy * 4 + gl_LocalInvocationID.xy);
		s_srcTexels[THREAD_IDX] = texelFetch(txSrc, iuv, 0).rgb;
		groupMemoryBarrier();
	}
	#endif
	#if 0
	 // copy src block into registers
		vec3 srcTexels[16];
		for (int i = 0; i < 16; ++i) {
			srcTexels[i] = SRC_TEXEL(i);
		}
		#undef SRC_TEXEL
		#define SRC_TEXEL(i) srcTexels[i]
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

 // find indices
	#if INDICES_EUCLIDEAN
	{
		vec3 palette[4];
		palette[0] = ep1;
		palette[1] = ep0;
		palette[2] = 2.0/3.0 * palette[0] + 1.0/3.0 * palette[1];
		palette[3] = 1.0/3.0 * palette[0] + 2.0/3.0 * palette[1];
		#if 1
		 // pack/unpack the palette values = quantize palette so that texel indices are generated from the final result
		 // \todo it's not clear that this significantly improves the quality
			for (int i = 0; i < 4; ++i) {
				palette[i] = UnpackRGB565(PackRGB565(palette[i]));
			}
		#endif

		int idx = 0;
		float minErr = 999.0;
		for (int i = 0; i < 4; ++i) {
			float err = length2(SRC_TEXEL(THREAD_IDX) - palette[i]);
			if (err < minErr) {
				minErr = err;
				idx = i;
			}
			s_indices[THREAD_IDX] = idx;
		}
	}
	#else
	{
	 // project onto (ep1 - ep0)
		vec3 d = ep1 - ep0;
		float dlen = length(d);
		d /= dlen;
	 	vec3 src = SRC_TEXEL(THREAD_IDX);
		float idx = dot(src - ep0, d) / dlen;

	 // round to nearest palette index
		idx = round(saturate(idx) * 3.0);
		const uvec4 idxMap = uvec4(1, 3, 2, 0);
		s_indices[THREAD_IDX] = idxMap[uint(idx)];
	}
	#endif

 // encode block
	if (THREAD_IDX == 0) {
	 // pack palette indices
		for (int i = 0; i < 16; ++i) {
			block[1] = bitfieldInsert(block[1], s_indices[i], (i * 2), 2);
		}	
	 // write block data
		bfDst[gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x] = block;
	}
	
}