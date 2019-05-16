#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Volume.glsl"

#define FIXED_STEP_INTEGRAL           0
#define ENERGY_CONSERVING_INTEGRATION 1
#define FIXED_STEP_COUNT              256
#define MIN_STEP_LENGTH               0.1
#define TRANSMITTANCE_THRESHOLD       1e-2

#define TRAPEZOID_RULE 0
#define SIMPSONS_RULE  0

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

const float kShapeScale   = 0.05;
const float kErosionScale = kShapeScale * 4.0;
const float kErosionStrength = 0.9;

	vec4 cloudControl = Volume_GetCloudControl(_p);

	float noiseShape = textureLod(txNoiseShape, _p * kShapeScale, _lod).x;
	density = Clouds_Remap(noiseShape, 1.0 - cloudControl.y, 1.0, 0.0, 1.0);

	float noiseErosion  = textureLod(txNoiseErosion, _p * kErosionScale, _lod).x;
	density = Clouds_Remap(density, saturate(noiseErosion * kErosionStrength), 1.0, 0.0, 1.0);

 // fade at the box edges
	vec3 edge = abs(Volume_GetNormalizedPosition(_p) * 2.0 - 1.0);
	density *= (1.0 - smoothstep(0.7, 0.9, max3(edge.x, edge.y, edge.z)));

	return density * uVolumeData.m_density;
}

void main()
{
 // discard any redundant thread invocations
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
			#if ENERGY_CONSERVING_INTEGRATION
			{
			 // Sebastien Hillaire, energy-conserving integration
				float s = muS ;// * Volume_Phase(VdotL);
				float si = (s - s * exp(-muE * stp)) / muE; // integrate wrt current step segment
				#if defined(SIMPSONS_RULE)
					float w = (mod(stpi - 1, 3) == 2) ? 2.0: 3.0;
					w = (stpi == 0 || stpi == (stpCount - 1)) ? 1.0 : w;
				#elif defined(TRAPEZOID_RULE)
					float w = (stpi == 0 || stpi == (stpCount - 1)) ? 1.0 : 2.0;
				#else
					float w = 1.0;
				#endif
				scatter = scatter + (si * Volume_GetNormalizedPosition(p)) * transmittance * w;
				transmittance *= exp(-muE * stp);
			}
			#else
			{
				scatter = scatter + (muS * Volume_GetNormalizedPosition(p)) * transmittance * stp ;// * Volume_Phase(VdotL);
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
