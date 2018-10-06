/*	GBuffer layout:

	               R                G                 B                A
	       +----------------+----------------+----------------+----------------+
    0      |             Albedo/reflectance  RGB              |                |
	       +----------------+----------------+----------------+----------------+
	1      |     Rough      |     Metal      |       AO       |                |
	       +----------------+----------------+----------------+----------------+
	2      |             Normal XY           |           Velocity XY           |
	       +- - - - - - - - +- - - - - - - - +- - - - - - - - +- - - - - - - - +

	- Normals are stored in view space.
	- Gamma correction of color data is performed on fetch (the data in 0 RGB is in gamma space).
*/

#include "shaders/common.glsl"

vec2 GBuffer_EncodeNormal(in vec3 normal_)
{
 // Lambert azimuthal projection
	float p = sqrt(clamp(normal_.z, -1.0, 1.0) * 8.0 + 8.0);
	return normal_.xy / p + 0.5;
}
vec3 GBuffer_DecodeNormal(in vec2 _normal)
{
 // Lambert azimuthal projection
	vec2 fenc = _normal.xy * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(max(1.0 - f / 4.0, 0.0));
	vec3 ret;
	ret.xy = fenc * g;
	ret.z = 1.0 - f / 2.0;
	return normalize(ret);
}

#if PASS_gbuffer && defined(FRAGMENT_SHADER)
	layout(location=0) out vec4 fGBuffer0; // albedo
	layout(location=1) out vec4 fGBuffer1; // material
	layout(location=2) out vec4 fGBuffer2; // normals

	void GBuffer_ExportAlbedo(in vec3 _albedo)
	{
		fGBuffer0 = vec4(_albedo, 0.0);
	}
	void GBuffer_ExportRoughnessMetalAo(in float _roughness, in float _metal, in float _ao)
	{
		fGBuffer1 = vec4(_roughness, _metal, _ao, 0.0);
	}
	void GBuffer_ExportNormal(in vec3 _normal)
	{
		fGBuffer2.xy = GBuffer_EncodeNormal(_normal);
	}
#endif