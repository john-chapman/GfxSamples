#include "shaders/def.glsl"
#include "shaders/sampling.glsl"

#define USE_SHARED_MEMORY  1  // Load src block into shared memory (else call texelFetch() for each sample).
#define USE_DITHER         0
#ifndef USE_PCA
	#define USE_PCA        0  // Use principle component analysis to find the block endpoints (else use the color space extents).
#endif

uniform sampler2D txSrc;

layout(std430) restrict writeonly buffer _bfDst
{
	uvec2 bfDst[];
};


#define TEXEL_INDEX (gl_LocalInvocationIndex)
#define TEXEL_COORD (gl_LocalInvocationID.xy)
#define BLOCK_INDEX (gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x)
#define BLOCK_COORD (gl_WorkGroupID.xy * 4)

#if USE_SHARED_MEMORY
	shared vec3 s_srcTexels[16];
	#define SRC_TEXEL(_i) s_srcTexels[_i]
#else
	#define SRC_TEXEL(_i) texelFetch(txSrc, ivec2(BLOCK_COORD) + ivec2(_i % 4, _i / 4), 0).rgb
#endif

shared uint s_indices[16];

void Swap(inout uint _a_, inout uint _b_)
{
	uint tmp = _a_;
	_a_ = _b_;
	_b_ = _a_;
}

void Swap(inout vec3 _a_, inout vec3 _b_)
{
	vec3 tmp = _a_;
	_a_ = _b_;
	_b_ = _a_;
}

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

vec3 FindPrincipleAxis(in vec3 _min, in vec3 _max, in vec3 _avg)
{
 // compute the covariance matrix (it's symmetrical, only need 6 elements)
	float cov[6];
	cov[0] = cov[1] = cov[2] = cov[3] = cov[4] = cov[5] = 0.0;
	for (int i = 0; i < 16; ++i) 
	{
		vec3 data = SRC_TEXEL(i) - _avg;
		cov[0] += data.x * data.x;
		cov[1] += data.x * data.y;
		cov[2] += data.x * data.z;
		cov[3] += data.y * data.y;
		cov[4] += data.y * data.z;
		cov[5] += data.z * data.z;
	}
	//vec3 axis = _max - _min;
	vec3 axis = vec3(0.9, 1.0, 0.7); // from Crunch, more stable?

 // principle axis via power method
	for (int i = 0; i < 8; ++i) 
	{
		vec3 estimate = vec3(
			dot(axis, vec3(cov[0], cov[1], cov[2])),
			dot(axis, vec3(cov[1], cov[3], cov[4])),
			dot(axis, vec3(cov[2], cov[4], cov[5]))
			);

		float mx = max3(abs(estimate.x), abs(estimate.y), abs(estimate.z));
		estimate = estimate * (1.0 / max(mx, 1e-7));
		
		float delta = length2(axis - estimate);
		axis = estimate;
		if (i > 2 && delta < 1e-6)
		{
			break;
		}
	}

	float len = length2(axis);
	if (len > 1e-7) 
	{
		axis = axis * (1.0 / sqrt(len));
	} 
	else 
	{
		axis = _max - _min;
	}

	return axis;
}

void SelectEndpoints(in vec3 _axis, in vec3 _avg, out vec3 ep0_, out vec3 ep1_)
{
#if 0
 // project onto vf and choose the extrema (from stb_dxt)
	float mind, maxd;
	mind = maxd = dot(_axis, SRC_TEXEL(0) - _avg);
	for (int i = 1; i < 16; ++i) 
	{
		float d = dot(_axis, SRC_TEXEL(i) - _avg);
		if (d < mind) 
		{
			ep0_ = SRC_TEXEL(i);
			mind = d;
		}
		if (d > maxd) 
		{
			ep1_ = SRC_TEXEL(i);
			maxd = d;
		}
	}
#else
 // less branchy
 // note that this is probably better as the result is guaranteed to pass through the mean, which in most cases should reduce overall error
	float mind, maxd;
	mind = maxd = dot(_axis, SRC_TEXEL(0) - _avg);
	for (int i = 1; i < 16; ++i) 
	{
		float d = dot(_axis, SRC_TEXEL(i) - _avg);
		mind = min(mind, d);
		maxd = max(maxd, d);
	}
	ep0_ = _avg + mind * _axis;
	ep1_ = _avg + maxd * _axis;
#endif

	ep0_ = saturate(ep0_);
	ep1_ = saturate(ep1_);
}

void main()
{
	#if USE_SHARED_MEMORY
	{
	 // load src block into shared memory (1 fetch per thread)
		ivec2 iuv = ivec2(BLOCK_COORD + TEXEL_COORD);
		s_srcTexels[TEXEL_INDEX] = texelFetch(txSrc, iuv, 0).rgb;
		memoryBarrierShared();
		barrier();
	}
	#endif

 // find endpoints
	vec3 ep0 = vec3(1.0);
	vec3 ep1 = vec3(0.0);
	#if USE_PCA
	{
		vec3 texelMin, texelMax, texelAvg;
		texelMin = texelMax = texelAvg = SRC_TEXEL(0);
		for (int i = 1; i < 16; ++i)
		{
			texelMin  = min(texelMin, SRC_TEXEL(i));
			texelMax  = max(texelMax, SRC_TEXEL(i));
			texelAvg += SRC_TEXEL(i);
		}
		texelAvg = texelAvg * (1.0/16.0);

		ep0 = texelMin;
		ep1 = texelMax;
		if (length2(texelMax - texelMin) > (2.0/255.0)) // \optim skip PCA for low-variance blocks
		{
			vec3 axis = FindPrincipleAxis(texelMin, texelMax, texelAvg);
			SelectEndpoints(axis, texelAvg, ep0, ep1);
		}
	}
	#else
	{
	 // color space extents
		ep0 = ep1 = SRC_TEXEL(0);
		for (int i = 1; i < 16; ++i) 
		{
			ep0 = min(ep0, SRC_TEXEL(i));
			ep1 = max(ep1, SRC_TEXEL(i));
		}
	}
	#endif

 // export endpoints
	uint ep0i = PackRGB565(ep0);	
	uint ep1i = PackRGB565(ep1);
	if (USE_PCA == 1 && ep0i > ep1i) // only need to swap if using PCA
	{
		Swap(ep0, ep1);
		Swap(ep0i, ep1i);
	}
	uvec2 block = uvec2(0);
	block[0] = bitfieldInsert(block[0], ep1i, 0,  16);
	block[0] = bitfieldInsert(block[0], ep0i, 16, 16);

 // early-out solid color blocks
 	if (ep0i == ep1i) 
	{
		if (TEXEL_INDEX == 0) 
		{
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
		d /= dlen;
	 	vec3 src = SRC_TEXEL(TEXEL_INDEX);
		float idx = dot(src - ep0, d) / dlen;

	 // round to nearest index
		#if USE_DITHER
		{
			ivec2 iuv = ivec2(BLOCK_COORD + TEXEL_COORD);
			float bayer = Bayer_4x4(uvec2(iuv));
			idx = saturate(idx) * 3.0;
			idx = floor(idx) + (saturate(idx - floor(idx)) < bayer ? 0.0 : 1.0);
		}
		#else
		{
			idx = round(saturate(idx) * 3.0);
		}
		#endif
		const uvec4 idxMap = uvec4(1, 3, 2, 0);
		s_indices[TEXEL_INDEX] = idxMap[uint(idx)];
	}
	memoryBarrierShared();
	barrier();

 // encode block
	if (TEXEL_INDEX == 0) 
	{
	 // pack palette indices
		for (int i = 0; i < 16; ++i) 
		{
			block[1] = bitfieldInsert(block[1], s_indices[i], i * 2, 2);
		}	
	 // write block data
		bfDst[BLOCK_INDEX] = block;
	}
	
}