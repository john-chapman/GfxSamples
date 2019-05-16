#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Volume.glsl"

#define FIXED_STEP_INTEGRAL           0
#define ENERGY_CONSERVING_INTEGRATION 1
#define FIXED_STEP_COUNT              64
#define MIN_STEP_LENGTH               0.1
#define TRANSMITTANCE_THRESHOLD       1e-2
#define TRAPEZOID_RULE 0
#define SIMPSONS_RULE  0
#define SHADOW_STEP_SCALE             4.0

layout(rgba16f) uniform image2D txDst;

layout(std430) restrict readonly buffer bfVolumeData
{
	VolumeData uVolumeData;
};

uniform sampler3D txNoiseShape;
uniform sampler3D txNoiseErosion;
uniform sampler2D txCloudControl;


// Remap _x from [_min0,_max0] to [_min1,_max1].
float Clouds_Remap(in float _x, in float _min0, in float _max0, in float _min1, in float _max1)
{
	return _min1 + (((_x - _min0) / (_max0 - _min0)) * (_max1 - _min1));
}

vec3 Volume_GetNormalizedPosition(in vec3 _p)
{
	return (_p - uVolumeData.m_volumeExtentMin.xyz) /  (uVolumeData.m_volumeExtentMax.xyz - uVolumeData.m_volumeExtentMin.xyz);
}

vec4 Volume_GetCloudControl(in vec3 _p)
{
	vec2 uv = Volume_GetNormalizedPosition(_p).xz;
	return textureLod(txCloudControl, uv, 0.0);
}

float Volume_GetDensity(in vec3 _p, in float _lod)
{
	float density = 0.0;

 // shape/erosion noise (clouds)
 // \todo enable a 'cheap' mode which applied erosion without sampling the high frequency texture
	vec4 cloudControl = Volume_GetCloudControl(_p);
	float cloudCoverage = saturate(cloudControl.y + uVolumeData.m_coverageBias);
	float noiseShape = textureLod(txNoiseShape, _p * uVolumeData.m_shapeScale, _lod).x;
	density = Clouds_Remap(noiseShape, 1.0 - cloudCoverage, 1.0, 0.0, 1.0);
	density = saturate(density);
	float noiseErosion  = textureLod(txNoiseErosion, _p * uVolumeData.m_erosionScale, _lod).x;
	density = Clouds_Remap(density, saturate(noiseErosion * uVolumeData.m_erosionStrength), 1.0, 0.0, 1.0);

 // fade at the box edges
	vec3 edge = abs(Volume_GetNormalizedPosition(_p) * 2.0 - 1.0);
	density = density * (1.0 - smoothstep(0.8, 0.9, max3(edge.x, edge.y, edge.z)));

	return density * uVolumeData.m_density;
}

float Volume_Phase(in float _VdotL)
{
	return 1.0;
}

float Volume_Shadow(in vec3 _p, in vec3 _dir)
{
	float transmittance = 1.0;
	float tmin, tmax;
	if (_IntersectRayBox(_p, _dir, uVolumeData.m_volumeExtentMin.xyz, uVolumeData.m_volumeExtentMax.xyz, tmin, tmax))
	{
		#if FIXED_STEP_INTEGRAL
			float stp = MIN_STEP_LENGTH;
		#else
			float stp = max((tmax - tmin) / float(FIXED_STEP_COUNT), MIN_STEP_LENGTH);
		#endif
		stp *= SHADOW_STEP_SCALE;

		float t = tmin + stp;
		while (t < tmax && transmittance > 0.1)
		{
			vec3 p = _p + _dir * t;
			float density = Volume_GetDensity(p, 0.0);
			float muS = density * uVolumeData.m_scatter;
			float muE = max(muS, 1e-7);
			transmittance *= exp(-muE * stp);
			t += stp;
		}
	}

	return saturate(transmittance);
}

void main()
{
	vec2 txSize = vec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(ivec2(gl_GlobalInvocationID.xy), ivec2(txSize))))
	{
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);

	vec4 ret = vec4(0.0);

	vec3 rayOrigin = Camera_GetPosition();
	vec3 rayDirection = Camera_GetViewRayW(uv * 2.0 - 1.0);
	float tmin, tmax;
	if (_IntersectRayBox(rayOrigin, rayDirection, uVolumeData.m_volumeExtentMin.xyz, uVolumeData.m_volumeExtentMax.xyz, tmin, tmax))
	{
		#if FIXED_STEP_INTEGRAL
			float stp = MIN_STEP_LENGTH;
			int stpCount = int((tmax - tmin) / stp);
		#else
			float stp = max((tmax - tmin) / float(FIXED_STEP_COUNT), MIN_STEP_LENGTH);
			int stpCount = FIXED_STEP_COUNT;
		#endif

		vec3 scatter = vec3(0.0);
		float transmittance = 1.0;
		float t = tmin;
		int stpi = 0;
		while (t < tmax && transmittance > TRANSMITTANCE_THRESHOLD)
		{
			vec3 p = rayOrigin + rayDirection * t;
			float density = Volume_GetDensity(p, 0.0);
			float muS = density * uVolumeData.m_scatter;
			float muE = max(muS, 1e-7);
			float shadow = Volume_Shadow(p, uVolumeData.m_lightDirection.xyz);
			#if ENERGY_CONSERVING_INTEGRATION
			{
			 // Sebastien Hillaire, energy-conserving integration
				float s = muS;
				float si = (s - s * exp(-muE * stp)) / muE; // integrate wrt current step segment
				#if defined(SIMPSONS_RULE)
					float w = (mod(stpi - 1, 3) == 2) ? 2.0: 3.0;
					w = (stpi == 0 || stpi == (stpCount - 1)) ? 1.0 : w;
				#elif defined(TRAPEZOID_RULE)
					float w = (stpi == 0 || stpi == (stpCount - 1)) ? 1.0 : 2.0;
				#else
					float w = 1.0;
				#endif
				si = si * shadow * Volume_Phase(dot(rayDirection, uVolumeData.m_lightDirection.xyz));
				scatter = scatter + (si * Volume_GetNormalizedPosition(p)) * transmittance * w;
				transmittance *= exp(-muE * stp);
			}
			#else
			{
				scatter = scatter + (muS * Volume_GetNormalizedPosition(p)) * transmittance * stp;
				transmittance *= exp(-muE * stp);
			}
			#endif

			t = t + stp;
			++stpi;
		}
		#if defined(SIMPSONS_RULE)
			scatter *= 3.0 / 8.0;
		#elif defined(TRAPEZOID_RULE)
			scatter *= 0.5;
		#endif
		
		ret = vec4(scatter, 1.0);
	}

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), vec4(ret));
}
