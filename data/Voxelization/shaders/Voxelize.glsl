#include "shaders/def.glsl"
#include "shaders/common.glsl"
#include "shaders/Color.glsl"

uniform mat4  uWorld;
uniform vec3  uVolumeOrigin;
uniform vec3  uVolumeSizeMeters;
uniform ivec3 uVolumeSizeVoxels;

layout(std430) buffer bfVoxelVolume
{
	uint uVoxelData[];
};

#define VertexData \
	VertexData { \
		noperspective vec3 m_normalW; \
		noperspective vec3 m_positionW; \
		noperspective vec3 m_positionV; \
		noperspective vec3 m_color; \
	}

uint PackVoxelData(in vec3 _rgb)
{
	if (all(lessThanEqual(_rgb, vec3(1e-7))))
	{
		return 0;
	}

	#if ENCODE_RGBM
	{
		uint ret = 0;
		vec4 rgbm = Color_RGBMEncode(_rgb);
		ret = bitfieldInsert(ret, 1,                    0,  1);
		ret = bitfieldInsert(ret, uint(rgbm.a * 127.0), 1,  7);
		ret = bitfieldInsert(ret, uint(rgbm.b * 255.0), 8,  8);
		ret = bitfieldInsert(ret, uint(rgbm.g * 255.0), 16, 8);
		ret = bitfieldInsert(ret, uint(rgbm.r * 255.0), 24, 8);
		return ret;
	}
	#else
	{
		uint ret = 1;
		ret = bitfieldInsert(ret, uint(_rgb.b * 255.0), 8,  8);
		ret = bitfieldInsert(ret, uint(_rgb.g * 255.0), 16, 8);
		ret = bitfieldInsert(ret, uint(_rgb.r * 255.0), 24, 8);
		return ret;
	}
	#endif
}

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
	vData.m_positionV = (vData.m_positionW - uVolumeOrigin) / (uVolumeSizeMeters * 0.5);
	vData.m_color     = aPosition * 0.5 + 0.5;
	gl_Position       = vec4(vData.m_positionV, 1.0);
	#ifdef FRM_NDC_Z_ZERO_TO_ONE
		gl_Position.z = gl_Position.z * 0.5 + 0.5;
	#endif
}
#endif // VERTEX_SHADER

#ifdef GEOMETRY_SHADER /////////////////////////////////////////////////////////

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in  VertexData vData[];
out VertexData vDataOut;

void main()
{
 // select the greatest component of the face normal input[3] is the input array of three vertices
	vec3 normal = abs(vData[0].m_normalW + vData[1].m_normalW + vData[2].m_normalW);
 	uint maxi = normal[1] > normal[0] ? 1 : 0;
	     maxi = normal[2] > normal[maxi] ? 2 : maxi;

	vec4 clipSpacePositions[3];
	for (uint i = 0; i < 3; ++i)
	{
	 // project voxel-space pos onto dominant axis
		clipSpacePositions[i] = gl_in[i].gl_Position.xyzw;
		if (maxi == 0)
		{
			clipSpacePositions[i].xyz = clipSpacePositions[i].zyx;
		}
		else if (maxi == 1)
		{
			clipSpacePositions[i].xyz = clipSpacePositions[i].xzy;
		}
		clipSpacePositions[i].z = 0.0; // don't clip
	}

	// Conservative rasterization (just expand the triangle a bit)
 vec2 side0N = normalize(clipSpacePositions[1].xy - clipSpacePositions[0].xy);
 vec2 side1N = normalize(clipSpacePositions[2].xy - clipSpacePositions[1].xy);
 vec2 side2N = normalize(clipSpacePositions[0].xy - clipSpacePositions[2].xy);
 const float texelSize = 1.0 / float(max3(uVolumeSizeVoxels.x, uVolumeSizeVoxels.y, uVolumeSizeVoxels.z));
 const float kExpansion = 1.0;
 clipSpacePositions[0].xy += normalize(side2N - side0N)*texelSize * kExpansion;
 clipSpacePositions[1].xy += normalize(side0N - side1N)*texelSize * kExpansion;
 clipSpacePositions[2].xy += normalize(side1N - side2N)*texelSize * kExpansion;

	for (uint i = 0; i < 3; ++i)
	{
		gl_Position          = clipSpacePositions[i];
		vDataOut.m_normalW   = vData[i].m_normalW;
		vDataOut.m_positionW = vData[i].m_positionW;
		vDataOut.m_positionV = vData[i].m_positionV;
		vDataOut.m_color     = vData[i].m_color;
		EmitVertex();
	}
}
#endif // GEOMETRY_SHADER

#ifdef FRAGMENT_SHADER /////////////////////////////////////////////////////////

in VertexData vData;

void main()
{
	ivec3 voxelCoord = ivec3((vData.m_positionV * 0.5 + 0.5) * vec3(uVolumeSizeVoxels));
	int voxelIndex = voxelCoord.z * (uVolumeSizeVoxels.x * uVolumeSizeVoxels.y) + voxelCoord.y * (uVolumeSizeVoxels.x) + voxelCoord.x;
	float NdotL = max(0.2, dot(normalize(vData.m_normalW), normalize(vec3(1.0))));

	int voxelIndexMax = (uVolumeSizeVoxels.x * uVolumeSizeVoxels.y * uVolumeSizeVoxels.z) - 1;
	if (voxelIndex > 0 && voxelIndex < voxelIndexMax)
	{
		vec3 ret = NdotL * vec3(vData.m_color);
		atomicMax(uVoxelData[voxelIndex], PackVoxelData(ret));
	}
}
#endif // FRAGMENT_SHADER
