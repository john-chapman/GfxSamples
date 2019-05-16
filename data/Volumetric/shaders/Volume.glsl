struct VolumeData
{
	vec4   m_volumeExtentMin;
	vec4   m_volumeExtentMax;
	vec4   m_lightDirection;

	float  m_coverageBias;
	float  m_density;
	float  m_scatter;
	float  m_shapeScale;
	float  m_erosionScale;
	float  m_erosionStrength;
};

bool _SolveQuadratic(in float _a, in float _b, in float _c, out float x0_, out float x1_) 
{
	float d = _b * _b - 4.0 * _a * _c; 
    if (d <= 0.0) 
	{
		return false;
	}
	d = sqrt(d);
	float q = 0.5 * (_b + sign(_b) * d);
	x0_ = _c / q;
	x1_ = q / _a;
	return true; 
}

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

bool _IntersectRaySphere(in vec3 _rayOrigin, in vec3 _rayDirection, in vec3 _sphereOrigin, in float _sphereRadius, out float tmin_, out float tmax_)
{
	vec3  p = _sphereOrigin - _rayOrigin;
	float a = 1.0;//length2(_r.m_direction);
	float b = 2.0 * dot(_rayDirection, p);
	float c = length2(p) - (_sphereRadius * _sphereRadius);
	if (!_SolveQuadratic(a, b, c, tmin_, tmax_))
	{
		return false;
	}
	if (tmin_ < 0.0 && tmax_ < 0.0) // sphere behind ray origin
	{
		return false;
	} 
	else if (tmin_ < 0.0 || tmax_ < 0.0) // ray origin inside sphere
	{
		float t = max(tmin_, tmax_);
		tmin_ = tmax_ = t;
	}
	return true;
}
