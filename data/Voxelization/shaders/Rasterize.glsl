#include "shaders/def.glsl"
#include "shaders/common.glsl"
#include "shaders/Camera.glsl"

#define VertexData \
	VertexData { \
		smooth vec3 m_normalW; \
		smooth vec3 m_positionW; \
	}

uniform mat4  uWorld;
uniform vec3  uVolumeOrigin;
uniform vec3  uVolumeSizeMeters;
uniform ivec3 uVolumeSizeVoxels;

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

layout(location=0) in vec3  aPosition;
layout(location=1) in vec3  aNormal;
layout(location=2) in vec3  aTangent;
layout(location=3) in vec2  aTexcoord;

out VertexData vData;

void main()
{
	vData.m_normalW   = TransformDirection(uWorld, aNormal);
	vData.m_positionW = TransformPosition(uWorld, aPosition);
	gl_Position       = bfCamera.m_viewProj * vec4(vData.m_positionW, 1.0);
}
#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

in VertexData vData;
out vec4 fResult;

void main()
{
	float NdotL = max(0.1, dot(normalize(vData.m_normalW), normalize(vec3(1.0))));
	fResult = vec4(vec3(NdotL), 0.5);

	
	//fResult.rgb = ((vData.m_positionW - uVolumeOrigin) / (uVolumeSizeMeters * 0.5));
	//fResult.a = 1.0;

	//ivec3 voxelCoord = ivec3((fResult.rgb * 0.5 + 0.5) * vec3(uVolumeSizeVoxels));
	//int voxelIndex = voxelCoord.z * (uVolumeSizeVoxels.x * uVolumeSizeVoxels.y) + voxelCoord.y * (uVolumeSizeVoxels.x) + voxelCoord.x;
	//fResult.rgb = vec3(voxelCoord) / vec3(uVolumeSizeVoxels);
	//fResult.rgb = vec3(float(voxelIndex) / float(uVolumeSizeVoxels.x * uVolumeSizeVoxels.y * uVolumeSizeVoxels.z));

}
#endif // FRAGMENT_SHADER
