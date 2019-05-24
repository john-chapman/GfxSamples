#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Octree.glsl"

layout(rgba8) uniform image2D txDst;

uniform vec3 uOctreeOrigin;
uniform vec3 uOctreeScale;

bool _IntersectRayBox(in vec3 _rayOrigin, in vec3 _rayDirection, in vec3 _boxMin, in vec3 _boxMax, out float tmin_, out float tmax_)
{
	vec3 omin = (_boxMin - _rayOrigin) / _rayDirection;
	vec3 omax = (_boxMax - _rayOrigin) / _rayDirection;
	vec3 tmax = max(omax, omin);
	vec3 tmin = min(omax, omin);
	tmax_ = min(tmax.x, min(tmax.y, tmax.z));
	tmin_ = max(max(tmin.x, 0.0), max(tmin.y, tmin.z));
	return tmax_ > tmin_;
}

void main()
{
	const vec3 kLevelColors[] =
	{
		vec3(1.0, 0.0, 0.0),
		vec3(0.0, 1.0, 0.0),
		vec3(0.0, 0.0, 1.0),
		vec3(1.0, 0.0, 1.0),
		vec3(1.0, 1.0, 0.0),
		vec3(0.0, 1.0, 1.0)
	};

 	vec2 txSize = vec2(imageSize(txDst).xy);
	if (any(greaterThanEqual(ivec2(gl_GlobalInvocationID.xy), ivec2(txSize)))) 
	{
		return;
	}
	vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(txSize) + 0.5 / vec2(txSize);
	
	vec3 rayOrigin = Camera_GetPosition();
	vec3 rayDirection = Camera_GetViewRayW(uv * 2.0 - 1.0);
	vec4 ret = vec4(0.0, 0.0, 0.0, 1.0);
	float tmin, tmax;
	if (_IntersectRayBox(rayOrigin, rayDirection, uOctreeOrigin - uOctreeScale, uOctreeOrigin + uOctreeScale, tmin, tmax))
	{
		float stp = 0.5;
		float t = tmin + stp;
		int stpi = 0;
		//while (t < tmax)
		while (stpi < 64 && t < tmax)
		{
			vec3 p = rayOrigin + rayDirection * t;
			vec3 o = (p - uOctreeOrigin) / uOctreeScale;
			t += stp;
			++stpi;

			#if 0
			{
				uint nodeIndex = Octree_FindLeaf(o);
				if (nodeIndex != Octree_INVALID_NODE_INDEX)
				{
					float edgeDistance = 1.0 - smoothstep(0.05, 0.15, Octree_EdgeDistance(nodeIndex, o));
					vec3 color = 
						kLevelColors[Octree_GetNodeLevel(nodeIndex) % kLevelColors.length()]
						//vec3(float(Octree_GetNodeDataCount(nodeIndex)) / 10.0)
						;
					float s = edgeDistance;
					float muE = max(s, 1e-7);
					float si = (s - s * exp(-muE * stp)) / muE;
					ret.rgb += si * color * ret.a;
					ret.a *= exp(-muE * stp);
				}
			}
			#else
			{
				#if 1
					uint tstack[32];
					int tstackTop = 0;
					tstack[0] = 0;
					while (tstackTop != -1)
					{
						uint nodeIndex = tstack[tstackTop];
						--tstackTop;
						uint childCount = Octree_GetNodeChildCount(nodeIndex);
						uint childIndex = Octree_GetNodeChildIndex(nodeIndex);
						for (uint i = 0; i < childCount; ++i)
						{
							tstack[++tstackTop] = childIndex + i;
						}

						uint dataCount = Octree_GetNodeDataCount(nodeIndex);
						if (Octree_Contains(nodeIndex, o))
						{
							uint dataIndex = Octree_GetNodeDataIndex(nodeIndex);
							for (uint i = 0; i < dataCount; ++i)
							{
								vec4 nodeData = Octree_GetNodeData(dataIndex, i);
								float edgeDistance = 1.0 - smoothstep(0.75, 1.0, length(p - nodeData.xyz) / nodeData.w);
								vec3 color = vec3(nodeData.xyz / uOctreeScale) * 0.5 + 0.5;

								float s = edgeDistance;
								float muE = max(s, 1e-7);
								float si = (s - s * exp(-muE * stp)) / muE;
								ret.rgb += si * color * ret.a;
								ret.a *= exp(-muE * stp);
							}
						}
					}
				#else
				{
					for (uint dataIndex = 0; dataIndex < min(uNodeData.length(), 50); ++dataIndex)
					{
						vec4 nodeData = Octree_GetNodeData(dataIndex, 0);
						float edgeDistance = 1.0 - smoothstep(0.99, 1.0, length(p - nodeData.xyz) / nodeData.w);
						vec3 color = vec3(nodeData.xyz / uOctreeScale) * 0.5 + 0.5;
						float s = edgeDistance;
						float muE = max(s, 1e-7);
						float si = (s - s * exp(-muE * stp)) / muE;
						ret.rgb += si * color * ret.a;
						ret.a *= exp(-muE * stp);
					}
				}
				#endif
			}
			#endif
		}
	}

	imageStore(txDst, ivec2(gl_GlobalInvocationID.xy), vec4(ret));
}
