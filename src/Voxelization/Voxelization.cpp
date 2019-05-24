#include "Voxelization.h"

#include <frm/core/def.h>
#include <frm/core/Buffer.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/MeshData.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <apt/ArgList.h>

using namespace frm;
using namespace apt;

static Voxelization s_inst;

Voxelization::Voxelization()
	: AppBase("Voxelization") 
{
	PropertyGroup& propGroup = m_props.addGroup("Voxelization");
	//                  name                       default                   min       max          storage
	propGroup.addFloat ("m_voxelsPerMeter",        m_voxelsPerMeter,         0.1f,     2.0f,        &m_voxelsPerMeter);
	propGroup.addFloat3("m_voxelVolumeSizeMeters", m_voxelVolumeSizeMeters,  1.0f,     256.0f,      &m_voxelVolumeSizeMeters);
	propGroup.addFloat3("m_voxelVolumeOrigin",     m_voxelVolumeOrigin,      -FLT_MAX, FLT_MAX,     &m_voxelVolumeOrigin);
}

Voxelization::~Voxelization()
{
}

bool Voxelization::init(const apt::ArgList& _args)
{
	if (!AppBase::init(_args)) 
	{
		return false;
	}

	m_worldMatrix = ScaleMatrix(vec3(m_voxelVolumeSizeMeters.y / 2.0f));

	//m_msTeapot = Mesh::Create("models/box.obj");
	//m_msTeapot = Mesh::Create("models/teapot.obj");
	m_msTeapot = Mesh::Create("models/md5/bob_lamp_update.md5mesh");
	if (!m_msTeapot || m_msTeapot->getState() != Mesh::State_Loaded)
	{
		return false;
	}

	m_shCopyToVolume = Shader::CreateCs("shaders/CopyToVolume_cs.glsl", 4, 4, 4);
	if (!m_shCopyToVolume || m_shCopyToVolume->getState() != Shader::State_Loaded)
	{
		return false;
	}

	m_shVoxelize = Shader::CreateVsGsFs("shaders/Voxelize.glsl", "shaders/Voxelize.glsl", "shaders/Voxelize.glsl");
	//m_shVoxelize = Shader::CreateVsFs("shaders/Voxelize.glsl", "shaders/Voxelize.glsl");
	if (!m_shVoxelize || m_shVoxelize->getState() != Shader::State_Loaded)
	{
		return false;
	}

	m_shRasterize = Shader::CreateVsFs("shaders/Rasterize.glsl", "shaders/Rasterize.glsl");
	if (!m_shRasterize || m_shRasterize->getState() != Shader::State_Loaded)
	{
		return false;
	}

	m_shVolumeVis = Shader::CreateVsFs("shaders/VolumeVis.glsl", "shaders/VolumeVis.glsl");
	if (!m_shVolumeVis || m_shVolumeVis->getState() != Shader::State_Loaded)
	{
		return false;
	}

	if (!initVoxelVolume())
	{
		return false;
	}

	m_txSceneColor = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
	m_txSceneColor->setName("txSceneColor");
	m_txSceneDepth = Texture::Create2d(m_resolution.x, m_resolution.y, GL_DEPTH_COMPONENT32F);
	m_txSceneDepth->setName("txSceneDepth");
	m_fbScene = Framebuffer::Create(2, m_txSceneColor, m_txSceneDepth);

	return true;
}

void Voxelization::shutdown()
{
	shutdownVoxelVolume();

	Mesh::Release(m_msTeapot);
	Shader::Release(m_shVoxelize);
	Shader::Release(m_shCopyToVolume);
	Shader::Release(m_shVolumeVis);
	Texture::Release(m_txSceneColor);
	Texture::Release(m_txSceneDepth);
	Framebuffer::Destroy(m_fbScene);

	AppBase::shutdown();
}

bool Voxelization::update()
{
	if (!AppBase::update()) 
	{
		return false;
	}

	ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Volume"))
	{
		bool reinitVolume = false;

		reinitVolume |= ImGui::SliderFloat("Voxels/m", &m_voxelsPerMeter, 0.1f, 2.0f);
		reinitVolume |= ImGui::SliderFloat3("Size (m)", &m_voxelVolumeSizeMeters.x, 1.0f, 256.0f);
		ImGui::Text("Size (vx): %d, %d, %d", m_voxelVolumeSizeVoxels.x, m_voxelVolumeSizeVoxels.y, m_voxelVolumeSizeVoxels.z);
		//Im3d::GizmoTranslation("Volume Origin", &m_voxelVolumeOrigin.x);

		if (reinitVolume)
		{
			initVoxelVolume();
		}

		ImGui::TreePop();
	}

	Im3d::Gizmo("World Matrx", &m_worldMatrix.x.x);

	if (m_showHelpers)
	{
		Im3d::PushDrawState();
		Im3d::SetColor(Im3d::Color_Magenta);
		Im3d::SetSize(2.0f);
		Im3d::DrawAlignedBox(m_voxelVolumeOrigin - m_voxelVolumeSizeMeters * 0.5f, m_voxelVolumeOrigin + m_voxelVolumeSizeMeters * 0.5f);
		Im3d::PopDrawState();
	}

	return true;
}

void Voxelization::draw()
{
	auto ctx = GlContext::GetCurrent();

	Camera* drawCamera = Scene::GetDrawCamera();

	{	PROFILER_MARKER("Voxelize");

		glScopedEnable(GL_CULL_FACE, GL_FALSE);
		glAssert(glColorMask(0, 0, 0, 0));
		glAssert(glDepthMask(0));

	 // viewport size determines the number of fragment shaders we can dispatch which should be at least sqrt(voxelCount)
		int voxelCount = m_voxelVolumeSizeVoxels.x * m_voxelVolumeSizeVoxels.y * m_voxelVolumeSizeVoxels.z;
		int viewportSize = (int)Ceil(sqrt((float)voxelCount));
		ctx->setViewport(0, 0, viewportSize, viewportSize);
		ctx->setShader(m_shVoxelize);
		ctx->setMesh(m_msTeapot);
		ctx->setUniform("uWorld", m_worldMatrix);
		ctx->setUniform("uVolumeOrigin", m_voxelVolumeOrigin);
		ctx->setUniform("uVolumeSizeMeters", m_voxelVolumeSizeMeters); 
		ctx->setUniform("uVolumeSizeVoxels", m_voxelVolumeSizeVoxels);
		ctx->bindBuffer(m_bfVoxelVolume);
		ctx->draw();

		glAssert(glDepthMask(1));
		glAssert(glColorMask(1, 1, 1, 1));

		//glAssert(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT));
		glAssert(glMemoryBarrier(GL_ALL_BARRIER_BITS));
	}

	{	PROFILER_MARKER("Copy");

		ctx->setShader(m_shCopyToVolume);
		ctx->bindBuffer(m_bfVoxelVolume);
		ctx->bindImage("txVoxelVolume", m_txVoxelVolume, GL_WRITE_ONLY);
		ctx->dispatch(m_txVoxelVolume);

		//glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
		glAssert(glMemoryBarrier(GL_ALL_BARRIER_BITS));
	}

	{ 	PROFILER_MARKER("Volume Vis");

		ctx->setFramebufferAndViewport(m_fbScene);
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		glAssert(glPointSize(16.0f));
		glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		//glScopedEnable(GL_CULL_FACE, GL_TRUE);
		//glScopedEnable(GL_BLEND, GL_TRUE);
		
		ctx->setShader(m_shVolumeVis);
		ctx->setMesh(m_msVolumeVis);
		ctx->setUniform("uVolumeSizeMeters", m_voxelVolumeSizeMeters);
		ctx->setUniform("uVolumeOrigin", m_voxelVolumeOrigin);
		ctx->bindBuffer(m_bfVoxelVolume);
		ctx->bindBuffer(drawCamera->m_gpuBuffer);
		ctx->bindTexture(m_txVoxelVolume);
		ctx->draw();
	}

	if (ImGui::TreeNode("Ground Truth"))
	{
		glScopedEnable(GL_BLEND, GL_TRUE);
		//glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
		glScopedEnable(GL_CULL_FACE, GL_TRUE);

		ctx->setShader(m_shRasterize);
		ctx->setMesh(m_msTeapot);
		ctx->setUniform("uWorld", m_worldMatrix);
		ctx->setUniform("uVolumeOrigin", m_voxelVolumeOrigin); 
		ctx->setUniform("uVolumeSizeMeters", m_voxelVolumeSizeMeters);
		ctx->setUniform("uVolumeSizeVoxels", m_voxelVolumeSizeVoxels);
		ctx->draw();

		ImGui::TreePop();
	}

	ctx->blitFramebuffer(m_fbScene, nullptr);

	AppBase::draw();
}

// PRIVATE

bool Voxelization::initVoxelVolume()
{
	shutdownVoxelVolume();

	m_voxelVolumeSizeVoxels = ivec3(Floor(m_voxelVolumeSizeMeters * m_voxelsPerMeter));
	int voxelCount = m_voxelVolumeSizeVoxels.x * m_voxelVolumeSizeVoxels.y * m_voxelVolumeSizeVoxels.z;

	m_bfVoxelVolume = Buffer::Create(GL_SHADER_STORAGE_BUFFER, voxelCount * sizeof(uint32));
	m_bfVoxelVolume->setName("bfVoxelVolume");
	
	m_txVoxelVolume = Texture::Create3d(m_voxelVolumeSizeVoxels.x, m_voxelVolumeSizeVoxels.y, m_voxelVolumeSizeVoxels.z, GL_RGBA16F, 1);
	m_txVoxelVolume->setName("txVoxelVolume");
	m_txVoxelVolume->setWrap(GL_CLAMP_TO_EDGE);

	MeshBuilder msBuilder;
	for (int x = 0; x < m_voxelVolumeSizeVoxels.x; ++x)
	{
		for (int y = 0; y < m_voxelVolumeSizeVoxels.y; ++y)
		{
			for (int z = 0; z < m_voxelVolumeSizeVoxels.z; ++z)
			{
				MeshBuilder::Vertex vertex;
				vertex.m_position = vec3(x, y, z) / vec3(m_voxelVolumeSizeVoxels) + vec3(0.5f) / vec3(m_voxelVolumeSizeVoxels); // voxel centers
				msBuilder.addVertex(vertex);
			}
		}
	}
	MeshDesc msDesc(MeshDesc::Primitive_Points);
	msDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType_Float, 3);
	MeshData* msData = MeshData::Create(msDesc, msBuilder);
	m_msVolumeVis = Mesh::Create(*msData);
	MeshData::Destroy(msData);

	return true;
}

void Voxelization::shutdownVoxelVolume()
{
	Buffer::Destroy(m_bfVoxelVolume);
	Texture::Release(m_txVoxelVolume);
	Mesh::Release(m_msVolumeVis);
}
