#include "Raytrace.h"

#include <frm/core/frm.h>
#include <frm/core/rand.h>
#include <frm/core/geom.h>
#include <frm/core/ArgList.h>
#include <frm/core/Buffer.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>


#include <im3d/im3d.h>

using namespace frm;

static Raytrace s_inst;

static vec3 Reflect(const vec3& _N, const vec3& _I)
{
	return _I - (2.0f * Dot(_I, _N)) * _N;
}

Raytrace::Raytrace()
	: AppBase("Raytrace") 
{
	Properties::PushGroup("Raytrace");
		//              name                        default                     min           max     storage
		Properties::Add("m_sphereCount",            m_sphereCount,              1,            64,     &m_sphereCount);
		Properties::Add("m_maxChildRayCount",       m_maxChildRayCount,         0,            64,     &m_maxChildRayCount);
		Properties::Add("m_hitEpsilon",             m_hitEpsilon,               1e-7f,        0.1f,   &m_hitEpsilon);
		Properties::Add("m_resolutionScale",        m_resolutionScale,          1,            8,      &m_resolutionScale);
		Properties::Add("m_lightPath.m_theta",      m_lightPath.m_theta,        0.0f,         1.0f,   &m_lightPath.m_theta);
		Properties::Add("m_lightPath.m_azimuth",    m_lightPath.m_azimuth,      0.0f,         6.3f,   &m_lightPath.m_azimuth);
		Properties::Add("m_lightPath.m_elevation",  m_lightPath.m_elevation,    0.0f,         3.2f,   &m_lightPath.m_elevation);
	Properties::PopGroup();
}

Raytrace::~Raytrace()
{
	Properties::InvalidateGroup("Raytrace");
}

bool Raytrace::init(const ArgList& _args)
{
	if (!AppBase::init(_args)) 
	{
		return false;
	}

	m_shDebug = Shader::CreateCs("shaders/Debug_cs.glsl", 8, 8);
	if (!m_shDebug || m_shDebug->getState() != Shader::State_Loaded)
	{
		return false;
	}

	if (!initTexture())
	{
		return false;
	}

	if (!initMaterials())
	{
		return false;
	}

	if (!initScene())
	{
		return false;
	}

	return true;
}

void Raytrace::shutdown()
{
	shutdownScene();
	shutdownMaterials();
	shutdownTexture();

	Shader::Release(m_shDebug);

	AppBase::shutdown();
}

bool Raytrace::update()
{
	if (!AppBase::update()) 
	{
		return false;
	}

	bool reinitScene = false;
	reinitScene |= ImGui::SliderInt("Sphere Count", &m_sphereCount, 1, 128);
	if (reinitScene)
	{
		initScene();
	}
	ImGui::Spacing();
	ImGui::SliderInt("Max Ray Child Count", &m_maxChildRayCount, 0, 64);
	ImGui::DragFloat("Hit Epsilon", &m_hitEpsilon, 1e-7f, 1e-7f, 1e-1f, "%.6f");

	ImGui::Spacing();
	if (ImGui::SliderInt("Resolution Scale", &m_resolutionScale, 1, 8))
	{
		initTexture();
	}

	if (ImGui::TreeNode("Light Path"))
	{
		m_lightPath.m_radius = 8000.0f;
		m_lightPath.edit();

		ImGui::TreePop();
	}
	m_lightPath.apply((float)getDeltaTime());

	if (m_showHelpers)
	{
		Im3d::PushEnableSorting();

		Camera* cullCam = Scene::GetCullCamera();
		Ray primaryRay = createRay(cullCam->getPosition(), cullCam->getViewVector());
		debugTrace(primaryRay);

		Im3d::PushDrawState();
			Im3d::SetColor(Im3d::Color_Yellow);
			Im3d::SetSize(2.0f);
			Im3d::DrawAlignedBox(m_sceneBoundsMin, m_sceneBoundsMax);

			Im3d::SetSize(1.0f);
			for (Sphere& sphere : m_spheres)
			{
				vec3 color = m_materials[sphere.materialIndex].xyz();
				Im3d::SetColor(Im3d::Color(color.x, color.y, color.z));
				Im3d::DrawSphere(sphere.positionRadius.xyz(), sphere.positionRadius.w);
			}
		Im3d::PopDrawState();

		Im3d::PopEnableSorting();
	}

	return true;
}

void Raytrace::draw()
{
	GlContext* ctx = GlContext::GetCurrent();

	Camera* drawCamera = Scene::GetDrawCamera();

	{	PROFILER_MARKER("Debug");

		ctx->setShader(m_shDebug);
		ctx->setUniform("uHitEpsilon", m_hitEpsilon);
		ctx->setUniform("uLightDirection", m_lightPath.m_direction);
		ctx->bindBuffer(m_bfScene);
		ctx->bindBuffer(m_bfMaterials);
		ctx->bindBuffer(drawCamera->m_gpuBuffer);
		ctx->bindImage("txScene", m_txScene, GL_WRITE_ONLY);
		ctx->dispatch(m_txScene);
	}

	ctx->blitFramebuffer(m_fbScene, nullptr, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	AppBase::draw();
}

// PRIVATE

bool Raytrace::initScene()
{
	const float kSceneSize = 20.0f;
	const float kMinRadius = 0.5f;
	const float kMaxRadius = 2.0f;

	shutdownScene();

	m_sceneBoundsMin = vec3(FLT_MAX);
	m_sceneBoundsMax = vec3(-FLT_MAX); 
	Rand<> rnd;
	for (int i = 0; i < m_sphereCount; ++i)
	{
		vec3  position        = rnd.get<vec3>(vec3(-kSceneSize, 0.0f, -kSceneSize), vec3(kSceneSize, kSceneSize, kSceneSize));
		float radius          = rnd.get<float>(kMinRadius, kMaxRadius);
		int   materialIndex   = rnd.get<int>(0, (int)m_materials.size() - 1);
		
		Sphere& sphere        = m_spheres.push_back();
		sphere.positionRadius = vec4(position, radius);
		sphere.materialIndex  = materialIndex;
		m_sceneBoundsMin      = Min(m_sceneBoundsMin, position - vec3(radius));
		m_sceneBoundsMax      = Max(m_sceneBoundsMax, position + vec3(radius));
	}

	m_bfScene = Buffer::Create(GL_SHADER_STORAGE_BUFFER, (GLsizei)(sizeof(Sphere) * m_spheres.size()), 0, m_spheres.data());
	m_bfScene->setName("bfScene");

	return true;
}

void Raytrace::shutdownScene()
{
	m_spheres.clear();
	Buffer::Destroy(m_bfScene);
}

bool Raytrace::initMaterials()
{
	shutdownMaterials();

	m_materials.push_back(vec4(1.0f, 0.0f, 0.0f, 1.0f));
	m_materials.push_back(vec4(0.0f, 1.0f, 0.0f, 1.0f));
	m_materials.push_back(vec4(0.0f, 0.0f, 1.0f, 1.0f));
	m_materials.push_back(vec4(1.0f, 0.0f, 1.0f, 1.0f));
	m_materials.push_back(vec4(1.0f, 1.0f, 0.0f, 1.0f));
	m_materials.push_back(vec4(0.0f, 1.0f, 1.0f, 1.0f));
	m_materials.push_back(vec4(1.0f, 1.0f, 1.0f, 1.0f));

	m_bfMaterials = Buffer::Create(GL_SHADER_STORAGE_BUFFER, (GLsizei)(sizeof(Material) * m_materials.size()), 0, m_materials.data());
	m_bfMaterials->setName("bfMaterials");

	return true;
}

void Raytrace::shutdownMaterials()
{
	m_materials.clear();
	Buffer::Destroy(m_bfMaterials);
}

bool Raytrace::initTexture()
{
	shutdownTexture();
	
	ivec2 resolution = m_resolution / m_resolutionScale;
	resolution = Max(resolution, ivec2(1));

	m_txScene = Texture::Create2d(resolution.x, resolution.y, GL_RGBA8);
	m_txScene->setWrap(GL_CLAMP_TO_EDGE);
	m_txScene->setName("txScene");
	m_fbScene = Framebuffer::Create(1, m_txScene);

	return true;
}

void Raytrace::shutdownTexture()
{
	Texture::Release(m_txScene);
	Framebuffer::Destroy(m_fbScene);
}

void Raytrace::debugTrace(const Ray& _ray)
{
	PROFILER_MARKER_CPU("debugTrace");

	eastl::vector<Ray> rayStack;
	rayStack.push_back(_ray);

	int childCount = m_maxChildRayCount;
	while (!rayStack.empty())
	{
		Ray ray = rayStack.back();
		rayStack.pop_back();

		Hit hit = findClosestHit(ray);
		if (hit.sphere == nullptr)
		{
			Im3d::DrawLine(ray.origin /*+ ray.direction * ray.tmin*/, ray.origin + ray.direction * ray.tmax, 2.0f, Im3d::Color_Red);
			continue;
		}
	
		vec3 hitPosition = ray.origin + ray.direction * hit.t;
		Im3d::DrawLine(ray.origin /*+ ray.direction * ray.tmin*/, hitPosition, 2.0f, Im3d::Color_Green);
		Im3d::DrawPoint(hitPosition, 12.0f, Im3d::Color(1.0f, 1.0f, 1.0f, 0.5f));
		Im3d::DrawLine(hitPosition, hitPosition + hit.normal * 0.25f, 1.0f, Im3d::Color(1.0f, 1.0f, 1.0f, 0.5f));

		if (childCount > 0)
		{
			vec3 childOrigin    = ray.origin + ray.direction * (hit.t + m_hitEpsilon) + hit.normal * m_hitEpsilon;
			vec3 childDirection = Reflect(hit.normal, ray.direction);
			rayStack.push_back(createRay(childOrigin, childDirection));
			--childCount;
		}
	}
}

Raytrace::Ray Raytrace::createRay(const vec3& _origin, const vec3& _direction) const
{
	frm::Ray tmpRay(_origin, _direction);
	frm::AlignedBox sceneBox(m_sceneBoundsMin, m_sceneBoundsMax);
	float t0, t1;
	if (!Intersect(tmpRay, sceneBox, t0, t1))
	{
		return Ray();
	}
	Ray ret;
	ret.origin    = _origin;
	ret.direction = _direction;
	ret.tmin      = Max(0.0f, t0);
	ret.tmax      = Max(0.0f, t1);
	return ret;
}

Raytrace::Hit Raytrace::findClosestHit(const Ray& _ray) const
{
	Hit ret;
	for (const Sphere& sphere : m_spheres)
	{
		frm::Ray tmpRay(_ray.origin, _ray.direction);
		frm::Sphere tmpSphere(sphere.positionRadius.xyz(), sphere.positionRadius.w);

		float t0, t1;
		if (Intersect(tmpRay, tmpSphere, t0, t1))
		{
		 // \todo can use t1 if backface enabled
			if (t0 < ret.t)
			{
				ret.t      = t0;
				ret.sphere = &sphere;
				ret.normal = normalize((_ray.origin + _ray.direction * t0) - sphere.positionRadius.xyz());
			}
		}
	}
	return ret;
}
