#include "shaders/def.glsl"
#include "shaders/common.glsl"
#include "shaders/Color.glsl"

layout(std430) buffer bfVoxelVolume
{
	uint uVoxelData[];
};
uniform writeonly image3D txVoxelVolume;

vec4 UnpackVoxelData(in uint _data)
{
	if (bitfieldExtract(_data, 0, 1) == 0) // empty voxel
	{
		return vec4(0.0);
	}

	#if ENCODE_RGBM
	{
		vec4 rgbm;
		rgbm.a = float(bitfieldExtract(_data, 1,  7)) / 128.0;
		rgbm.b = float(bitfieldExtract(_data, 8,  8)) / 255.0;
		rgbm.g = float(bitfieldExtract(_data, 16, 8)) / 255.0;
		rgbm.r = float(bitfieldExtract(_data, 24, 8)) / 255.0;
		return vec4(Color_RGBMDecode(rgbm), 1.0);
	}
	#else
	{
		vec3 rgb;
		rgb.b = float(bitfieldExtract(_data, 8,  8)) / 255.0;
		rgb.g = float(bitfieldExtract(_data, 16, 8)) / 255.0;
		rgb.r = float(bitfieldExtract(_data, 24, 8)) / 255.0;
		return vec4(rgb, 1.0);
	}
	#endif
}

void main()
{
	ivec3 txSize = ivec3(imageSize(txVoxelVolume).xyz);
	if (any(greaterThanEqual(gl_GlobalInvocationID.xyz, txSize))) 
	{
		return;
	}
	
	uint voxelIndex = gl_GlobalInvocationID.z * (txSize.x * txSize.y) + gl_GlobalInvocationID.y * (txSize.x) + gl_GlobalInvocationID.x;
	vec4 ret = UnpackVoxelData(uVoxelData[voxelIndex]);
	uVoxelData[voxelIndex] = 0;

//vec3 uvw = Color_RGBMDecode(Color_RGBMEncode(vec3(gl_GlobalInvocationID.xyz) / vec3(txSize)));
//ret.rgb = uvw;

	imageStore(txVoxelVolume, ivec3(gl_GlobalInvocationID.xyz), ret);
}
