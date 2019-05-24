#include "shaders/def.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Octree.glsl"

layout(location=0) in vec3  aPosition;

uniform vec3 uOctreeOrigin;
uniform vec3 uOctreeScale;
uniform mat4 uWorld;

#ifdef VERTEX_SHADER //////////////////////////////////////////////////////////////////////////

smooth out float vDistance;
smooth out vec3 vColor;

void main() 
{
	vDistance = 0.0;
	vec3 posW = TransformPosition(uWorld, aPosition.xyz);
	vec3 posO = (posW - uOctreeOrigin) / uOctreeScale;
	vColor = posO * 0.5 + 0.5;
	#if 1
	{
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
			if (Octree_Contains(nodeIndex, posO))
			{
				uint dataIndex = Octree_GetNodeDataIndex(nodeIndex);
				for (uint i = 0; i < dataCount; ++i)
				{
					vec4 nodeData = Octree_GetNodeData(dataIndex, i);
					float edgeDistance = 1.0 - smoothstep(0.75, 1.0, length(posW - nodeData.xyz) / nodeData.w);
					vDistance += edgeDistance;
				}
			}
		}
	}
	#endif

	gl_Position = bfCamera.m_viewProj * vec4(posW, 1.0);
}

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER //////////////////////////////////////////////////////////////////////////

smooth in float vDistance;
smooth in vec3 vColor;

out vec4 fResult;

void main()
{
	fResult = vec4(max(vec3(0.1), vColor), saturate(vDistance + 0.1));
}

#endif // FRAGMENT_SHADER