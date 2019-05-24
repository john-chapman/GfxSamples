#include "OctreeGPU.h"

#include "Octree.h"

#include <frm/core/def.h>
#include <frm/core/geom.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/MeshData.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <apt/memory.h>
#include <apt/rand.h>
#include <apt/ArgList.h>

#include <im3d/im3d.h>

#include <EASTL/vector.h>

using namespace frm;
using namespace apt;

static OctreeGPU s_inst;

OctreeGPU::OctreeGPU()
	: AppBase("OctreeGPU") 
{
	PropertyGroup& propGroup = m_props.addGroup("OctreeGPU");
	//                  name                       default                  min       max          storage
	propGroup.addFloat ("m_octreeWidth",           m_octreeWidth,           1,        FLT_MAX,     &m_octreeWidth);
	propGroup.addFloat ("m_octreeHeight",          m_octreeHeight,          1,        FLT_MAX,     &m_octreeHeight);
	propGroup.addFloat3("m_octreeOrigin",          m_octreeOrigin,          -FLT_MAX, FLT_MAX,     &m_octreeOrigin);
}

OctreeGPU::~OctreeGPU()
{
}

bool OctreeGPU::init(const apt::ArgList& _args)
{
	if (!AppBase::init(_args)) 
	{
		return false;
	}

	int levelCount = 20;
	m_octree = APT_NEW(Octree(levelCount));

	m_txSceneColor = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
	m_txSceneColor->setName("txSceneColor");
	m_txSceneDepth = Texture::Create2d(m_resolution.x, m_resolution.y, GL_DEPTH_COMPONENT32F);
	m_txSceneDepth->setName("txSceneDepth");
	m_fbScene = Framebuffer::Create(2, m_txSceneColor, m_txSceneDepth);

	m_shOctreeVis = Shader::CreateCs("shaders/OctreeVis_cs.glsl", 8, 8);

	m_shPerVertex = Shader::CreateVsFs("shaders/PerVertex.glsl", "shaders/PerVertex.glsl");

	MeshDesc msDesc;
	msDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType_Float32, 3);
	MeshData* msData = MeshData::CreateSphere(msDesc, 2.0f, 256, 256);
	m_msPerVertex = Mesh::Create(*msData);
	MeshData::Destroy(msData);
	
	return true;
}

void OctreeGPU::shutdown()
{
	APT_DELETE(m_octree);

	Mesh::Release(m_msPerVertex);
	Shader::Release(m_shPerVertex);
	Texture::Release(m_txSceneColor);
	Framebuffer::Destroy(m_fbScene);
	Shader::Release(m_shOctreeVis);

	AppBase::shutdown();
}

bool OctreeGPU::update()
{
	if (!AppBase::update())
	{
		return false;
	}

	const vec3 octreeScale = vec3(m_octreeWidth, m_octreeHeight, m_octreeWidth);
	m_octree->update();

	if (ImGui::TreeNode("Octree"))
	{
		m_octree->debugDraw(m_octreeOrigin, octreeScale);
		ImGui::TreePop();
	}

	static Rand<> rnd;
	static eastl::vector<vec3> points;
	ImGui::Text("%u points", points.size());
	ImGui::Text("%u vertices", m_msPerVertex->getVertexCount());
	if (ImGui::Button("Insert 500 Points"))
	{
		for (int i = 0; i < 500; ++i)
		{
			vec3 insertionPoint = m_octreeOrigin + rnd.get<vec3>(-octreeScale, octreeScale);
			float radius = rnd.get<float>(0.25f, 2.0f);
			points.push_back(insertionPoint);
			vec3 origin = m_octreeOrigin + insertionPoint / octreeScale;
			AlignedBox box = AlignedBox(origin - vec3(radius) / octreeScale, origin + vec3(radius) / octreeScale);
			m_octree->insert(vec4(insertionPoint, radius), box);
		} 
	}

	if (ImGui::Button("Insert 500 Points (Snapped)"))
	{
		for (int i = 0; i < 500; ++i)
		{
			vec3 insertionPoint = Floor(m_octreeOrigin + rnd.get<vec3>(-octreeScale, octreeScale)) + vec3(0.5f);
			float radius = rnd.get<float>(0.05f, 0.05f);
			points.push_back(insertionPoint);
			vec3 origin = m_octreeOrigin + insertionPoint / octreeScale;
			AlignedBox box = AlignedBox(origin - vec3(radius) / octreeScale, origin + vec3(radius) / octreeScale);
			m_octree->insert(vec4(insertionPoint, radius), box);
		} 
	}

	/*static vec3 testPosition = vec3(0.0f);
	Im3d::GizmoTranslation("testPosition", &testPosition.x);
	if (ImGui::Button("Insert"))
	{
		points.push_back(testPosition);
		vec3 origin = m_octreeOrigin + testPosition / octreeScale;
		AlignedBox box = AlignedBox(origin - vec3(0.005f) / octreeScale, origin + vec3(0.005f) / octreeScale);
		m_octree->insert(vec4(testPosition, 1.0f), box); 
	}*/
	
	Im3d::PushDrawState();
	Im3d::SetSize(6.0f);
	Im3d::SetColor(Im3d::Color_White);
	Im3d::SetAlpha(0.5f); 
	Im3d::BeginPoints();
		for (auto& point : points)
		{
			Im3d::Vertex(point);
		}
	Im3d::End();
	Im3d::PopDrawState();
 
	#if 0
	{
		Im3d::PushDrawState();
		Camera* cullCamera = Scene::GetCullCamera();
		Ray ray(cullCamera->getPosition(), cullCamera->getViewVector());
		float tmin, tmax;
		if (Intersect(ray, AlignedBox(m_octreeOrigin - octreeScale, m_octreeOrigin + octreeScale), tmin, tmax))
		{
			Im3d::DrawLine(ray.m_origin + ray.m_direction * tmin, ray.m_origin + ray.m_direction * tmax, 2.0f, Im3d::Color_Blue);

			float stp = 0.5f;
			float t = tmin + stp;
			while (t < tmax)
			{
				vec3 p = ray.m_origin + ray.m_direction * t;
				auto nodeIndex = m_octree->debugFindLeaf((p - m_octreeOrigin) / octreeScale);
				Im3d::DrawPoint(p, 6.0f, (nodeIndex == 0) ? Im3d::Color_Blue : Im3d::Color_White);

				if (nodeIndex != 0)
				{
					vec4 centerWidth   = m_octree->debugGetCenterWidth(nodeIndex);
					vec3 nodeCenter    = m_octreeOrigin + centerWidth.xyz() * octreeScale;
					vec3 nodeHalfWidth = vec3(centerWidth.w / 2.0f) * octreeScale;
					vec3 nodeMin       = nodeCenter - nodeHalfWidth;
					vec3 nodeMax       = nodeCenter + nodeHalfWidth;
					Im3d::DrawAlignedBox(nodeMin, nodeMax);
				}
				t += stp;
			}
		}
		Im3d::PopDrawState();
	}
	#endif

	return true;
}

void OctreeGPU::draw()
{
	GlContext* ctx = GlContext::GetCurrent();
	
	Camera* drawCamera = Scene::GetDrawCamera();
	Camera* cullCamera = Scene::GetCullCamera();

	#if 0
	{	PROFILER_MARKER("Octree Vis");

		ctx->setShader(m_shOctreeVis);
		ctx->setUniform("uOctreeOrigin", m_octreeOrigin);
		ctx->setUniform("uOctreeScale", vec3(m_octreeWidth, m_octreeHeight, m_octreeWidth));
		ctx->bindBuffer(drawCamera->m_gpuBuffer);
		ctx->bindBuffer(m_octree->getHiearchyBufferGPU());
		ctx->bindBuffer(m_octree->getCenterWidthBufferGPU());
		ctx->bindBuffer(m_octree->getDataBufferGPU());
		ctx->bindBuffer(m_octree->getDataCountBufferGPU());
		ctx->bindImage("txDst", m_txSceneColor, GL_WRITE_ONLY);
		
		ctx->dispatch(m_txSceneColor);
	}
	#endif

	#if 1
	{	PROFILER_MARKER("Octree Per Vertex");

		static mat4 world = identity;
		Im3d::Gizmo("worldMatrix", &world.x.x);

		ctx->setFramebufferAndViewport(m_fbScene);
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		ctx->setShader(m_shPerVertex);
		ctx->setMesh(m_msPerVertex);
		ctx->setUniform("uOctreeOrigin", m_octreeOrigin);
		ctx->setUniform("uOctreeScale", vec3(m_octreeWidth, m_octreeHeight, m_octreeWidth));
		ctx->setUniform("uWorld", world);
		ctx->bindBuffer(drawCamera->m_gpuBuffer);
		ctx->bindBuffer(m_octree->getHiearchyBufferGPU());
		ctx->bindBuffer(m_octree->getCenterWidthBufferGPU());
		ctx->bindBuffer(m_octree->getDataBufferGPU());
		ctx->bindBuffer(m_octree->getDataCountBufferGPU());
		
		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glScopedEnable(GL_CULL_FACE, GL_TRUE); 
		glScopedEnable(GL_BLEND, GL_TRUE);
		ctx->draw();
	}
	#endif

	ctx->blitFramebuffer(m_fbScene, nullptr);
	
	AppBase::draw();
}
