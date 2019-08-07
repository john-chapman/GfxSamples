#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Raytrace.glsl"

uniform writeonly image2D txScene;

uniform float uHitEpsilon;
uniform vec3 uLightDirection;

layout(std430) restrict readonly buffer bfScene
{
	Sphere uSpheres[];
};

layout(std430) restrict readonly buffer bfMaterials
{
	Material uMaterials[];
};

Hit FindClosestHit(in Ray _ray)
{
	Hit ret;
	ret.t = FLT_MAX;
	for (uint i = 0; i < uSpheres.length(); ++i)
	{
		float t = 0.0;
		if (IntersectRaySphere(_ray, uSpheres[i], t) && t < ret.t)
		{
			ret.t = t;
			ret.primitiveId = i;
		}
	}
	ret.normal = normalize((_ray.origin + _ray.direction * ret.t) - uSpheres[ret.primitiveId].origin);
	return ret;
}

vec3 SampleBackground(in vec3 _v)
{
	return vec3(smoothstep(-1.0, 1.0, _v.y));
}

void main()
{
	ivec2 txSize = ivec2(imageSize(txScene).xy);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xy, txSize))) 
	{
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);

	Ray ray;
	ray.origin = Camera_GetPosition();
	ray.direction = Camera_GetViewRayW(uv * 2.0 - 1.0);

	vec4 ret = vec4(SampleBackground(ray.direction), 1.0);
	Hit hit = FindClosestHit(ray);
	if (hit.t != FLT_MAX)
	{
		//ret.rgb = hit.normal * 0.5 + 0.5;
		uint materialId = uSpheres[hit.primitiveId].materialId;
		ret.rgb = uMaterials[materialId].color.rgb;
		
		float NoL = saturate(dot(hit.normal, uLightDirection));
		ret.rgb *= NoL;

		if (NoL > 0.0)
		{
			Ray shadowRay;
			shadowRay.origin = ray.origin + ray.direction * (hit.t + uHitEpsilon) + hit.normal * uHitEpsilon;
			shadowRay.direction = uLightDirection;
			Hit shadowHit = FindClosestHit(shadowRay);
			if (shadowHit.t != FLT_MAX)
			{
				ret.rgb = vec3(0.0);
			}
		}

		ret.rgb += uMaterials[materialId].color.rgb * SampleBackground(hit.normal) * 0.2; // background ambient

		Ray childRay;
		childRay.origin = ray.origin + ray.direction * (hit.t + uHitEpsilon) + hit.normal * uHitEpsilon;
		childRay.direction = reflect(ray.direction, hit.normal);

		float NoV = pow(1.0 - saturate(dot(hit.normal, -ray.direction)), 2.0);
		hit = FindClosestHit(childRay);
		if (hit.t != FLT_MAX)
		{
			uint materialId = uSpheres[hit.primitiveId].materialId;
			ret.rgb += uMaterials[materialId].color.rgb * saturate(dot(hit.normal, uLightDirection)) * NoV;
		}
		else
		{
			ret.rgb += SampleBackground(childRay.direction) * NoV;
		}
	}

	imageStore(txScene, ivec2(gl_GlobalInvocationID.xy), ret);
}
