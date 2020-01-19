/*
Raytracing
==========
- Need to be able to 'pause' shader execution to wait for the results of tracing rays. Instead, use an event model:
    - OnHit: executed where the ray intersects the scene. This will probably modify the ray payload (e.g. evaluate lighting, etc.) and may spawn child rays.
    - OnMiss: executed where the ray does *not* intersect the scene.
    - OnComplete: executed when all child rays are terminated. This will probably gather the contribution from any child rays and then write a result.
  
  The shaders associated with these events are grouped into ray types (= DXR hit groups). Each ray has an atomic child counter; when a child ray terminates it decrements the
  parent's counter.



- Execute phases:
    0. Primary ray generation: allocate primary rays + data payload, push ray IDs into the active ray list.
    1. Ray evaluation: execute intersection shaders, generate OnHit and OnMiss events (= DXR intersection shader).
        This means pushing hit records into a buffer which will be the source for the next OnHit phase.
    2. Hit evaluation: execute the OnHit/OnMiss shaders, potentially generating OnComplete events (or more rays).
    3. Ray completion: execute OnComplete events.

    In theory phases 1-3 must run until *all* rays are complete (i.e. until phase 1 is an empty dispatch). In practice there will be a fixed number of iterations of phases 1-3,
    based on the total maxmimum number of child rays that can be generated.
        NB that child ray evaluation can't be deferred to a future frame since the primary ray phase will keep generating the same number of rays every frame.

*/
#pragma once

#include <frm/core/AppSample3d.h>
#include <frm/core/XForm.h>

#include <EASTL/vector.h>

typedef frm::AppSample3d AppBase;

using frm::vec4;
using frm::vec3;

class Raytrace: public AppBase
{
public:
	Raytrace();
	virtual ~Raytrace();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

 // scene
	frm::XForm_OrbitalPath m_lightPath;

	typedef vec4 Material;
	eastl::vector<Material> m_materials;
	frm::Buffer* m_bfMaterials = nullptr;

	int m_sphereCount = 16;
	struct Sphere
	{
		vec4 positionRadius   = vec4(0.0f);
		int  materialIndex    = 0;

		int  _pad[3]          = { 0 };
	};
	static_assert((sizeof(Sphere) % 16) == 0, "Sphere must be 16-byte aligned");
	eastl::vector<Sphere> m_spheres;
	vec3 m_sceneBoundsMin = vec3(0.0f);
	vec3 m_sceneBoundsMax = vec3(0.0f);
	frm::Buffer* m_bfScene = nullptr;
		
	bool initScene();
	void shutdownScene();	

	bool initMaterials();
	void shutdownMaterials();


	int m_resolutionScale = 4;
	frm::Texture* m_txScene = nullptr;
	frm::Framebuffer* m_fbScene = nullptr;
	frm::Shader* m_shDebug = nullptr;

	bool initTexture();
	void shutdownTexture();

 // rays
	struct Ray
	{
		vec3  origin     = vec3(0.0f);
		float tmin       = 0.0f;
		vec3  direction  = vec3(0.0f);
		float tmax       = 0.0f;
	};
	struct Hit
	{
		const Sphere* sphere = nullptr;
		vec3          normal = vec3(0.0f);
		float         t      = FLT_MAX;
	};
	float m_hitEpsilon       = 1e-7f;
	int   m_maxChildRayCount = 2;

	void debugTrace(const Ray& _ray);

	Ray createRay(const vec3& _origin, const vec3& _direction) const;
	Hit findClosestHit(const Ray& _ray) const;
};
