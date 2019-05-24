#include "shaders/def.glsl"
#include "shaders/common.glsl"
#include "shaders/Camera.glsl"
#include "shaders/Color.glsl"

uniform vec3 uVolumeOrigin;
uniform vec3 uVolumeSizeMeters;

layout(std430) buffer bfVoxelVolume
{
	uint uVoxelData[];
};
uniform sampler3D txVoxelVolume;

#ifdef VERTEX_SHADER ///////////////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;

smooth out vec3 vUvw;

void main()
{
	vUvw = aPosition;
	vec3 positionW = uVolumeOrigin + (aPosition - vec3(0.5)) * uVolumeSizeMeters;
	gl_Position = bfCamera.m_viewProj * vec4(positionW, 1.0);
}
#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

smooth in vec3 vUvw;

out vec4 fResult;

void main()
{
	fResult = textureLod(txVoxelVolume, vUvw, 0.0);
	if (fResult.a <= 1e-7)
	{
		discard;
	}
}
#endif // FRAGMENT_SHADER