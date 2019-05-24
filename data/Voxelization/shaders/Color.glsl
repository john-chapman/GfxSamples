#ifndef Color_glsl
#define Color_glsl

// http://graphicrants.blogspot.com/2009/04/rgbm-color-encoding.html
// RGBM encoding has a linear space range of 51.5.
// It is advisable to convert _rgb to gamma space before encoding.
vec4 Color_RGBMEncode(in vec3 _rgb)
{
	vec4 rgbm;
	_rgb     = _rgb * 1.0/6.0;
	rgbm.a   = saturate(max4(_rgb.r, _rgb.g, _rgb.b, 1e-7));
	rgbm.a   = ceil(rgbm.a * 255.0) / 255.0;
	rgbm.rgb = _rgb / rgbm.a;
	return rgbm;
}
vec3 Color_RGBMDecode(in vec4 _rgbm)
{
	return 6.0 * _rgbm.rgb * _rgbm.a;
}

#endif // Color_glsl
