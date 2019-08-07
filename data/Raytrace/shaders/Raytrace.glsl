#ifndef Raytrace
#define Raytrace

#ifndef FLT_MAX
    #define FLT_MAX 3.402823466e+38
#endif

struct Ray
{
    vec3  origin;
    float tmin;
    vec3  direction;
    float tmax;
};

struct Hit
{
    vec3  normal;
    float t;
    uint  rayId;
    uint  primitiveId;

    uint  _pad[2];
};

struct Sphere
{
    vec3  origin;
    float radius;
    int   materialId;

    uint  _pad[3];
};

struct Material
{
    vec4 color;
};

bool IntersectRaySphere(in Ray _ray, in Sphere _sphere, out float t_)
{
    vec3  sr = _ray.origin - _sphere.origin;
    float a  = 1.0;//length2(_ray.direction);
    float b  = 2.0 * dot(_ray.direction, sr);
    float c  = length2(sr) - _sphere.radius * _sphere.radius;
    if ((b * b - 4.0 * a * c) < 0.0)
    {
        return false;
    }
    t_ = (-b - sqrt((b * b) - 4.0 * a * c)) / (2.0 * a);
    return t_ > 0.0;
}

#endif // Raytrace
