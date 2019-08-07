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
}
