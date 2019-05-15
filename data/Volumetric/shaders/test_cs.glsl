#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Volume.glsl"

#define FIXED_STEP_INTEGRAL           0
#define ENERGY_CONSERVING_INTEGRATION 1
#define FIXED_STEP_COUNT              64
#define MIN_STEP_LENGTH               1e-3

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

vec4 Volume_GetCloudControl(in vec3 _p)
{
	vec2 uv = (_p.xz - uVolumeData.m_volumeExtentMin.xz) / (uVolumeData.m_volumeExtentMax.xz - uVolumeData.m_volumeExtentMin.xz);
	return textureLod(txCloudControl, uv, 0.0);
}

float Volume_GetDensity(in vec3 _p, in float _lod)
{
	float density = 0.0;

const float kShapeScale   = 0.03;
const float kErosionScale = kShapeScale * 8.0;
const float kErosionStrength = 0.5;

	vec4 cloudControl = Volume_GetCloudControl(_p);

	float noiseShape = textureLod(txNoiseShape, _p * kShapeScale, _lod).x;
	density = Clouds_Remap(noiseShape, 1.0 - cloudControl.y, 1.0, 0.0, 1.0);

	float noiseErosion  = textureLod(txNoiseErosion, _p * kErosionScale, _lod).x;
	density = Clouds_Remap(density, saturate(noiseErosion * kErosionStrength), 1.0, 0.0, 1.0);

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
		#else
			float stp = max((tmax - tmin) / float(FIXED_STEP_COUNT), MIN_STEP_LENGTH);
		#endif

		float scatter = 0.0;
		float transmittance = 1.0;
		float t = tmin + stp;
		while (t < tmax)
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
				float w = 1.0; // \todo simpson/trapezoid rule
				scatter = scatter + si * transmittance * w;
				transmittance *= exp(-muE * stp);
			}
			#else
			{
				scatter = scatter + muS * transmittance * stp * ;//Volume_Phase(VdotL);
				transmittance *= exp(-muE * stp);
			}
			#endif
			t = t + stp;
		}
		
		ret = vec4(scatter * (1.0 - transmittance));
	}

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), vec4(ret));
}
