/*	\todo
	- Need to decide whether a scene should collapse all of it's children, i.e. whether or not a scene contains Scene* nodes.
	- Simple cooker to generate the 'runtime' files (use JSON instead of BSON for the first version).
		- Watches data/SceneGraph.
		- Cooks to data/bin or something.
*/
#include "SceneGraph.h"

#include "SceneGraph/Material.h"
#include "SceneGraph/Model.h"

#include <frm/core/def.h>
#include <frm/core/gl.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Texture.h>

#include <apt/ArgList.h>

using namespace frm;
using namespace apt;

static SceneGraph s_inst;

SceneGraph::SceneGraph()
	: AppBase("SceneGraph") 
{
	PropertyGroup& propGroup = m_props.addGroup("SceneGraph");
	//                  name             default            min     max    storage
	//propGroup.addFloat  ("Float",        0.0f,              0.0f,   1.0f,  &foo);
}

SceneGraph::~SceneGraph()
{
}

bool SceneGraph::init(const apt::ArgList& _args)
{
	if (!AppBase::init(_args)) {
		return false;
	}

	m_txGBuffer0 = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
	m_txGBuffer0->setName("txGBuffer0");
	m_txGBuffer0->setFilter(GL_NEAREST);
	m_txGBuffer0->setWrap(GL_CLAMP_TO_EDGE);
	m_txGBuffer1 = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
	m_txGBuffer1->setName("txGBuffer1");
	m_txGBuffer1->setFilter(GL_NEAREST);
	m_txGBuffer1->setWrap(GL_CLAMP_TO_EDGE);
	m_txGBuffer2 = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
	m_txGBuffer2->setName("txGBuffer1");
	m_txGBuffer2->setFilter(GL_NEAREST);
	m_txGBuffer2->setWrap(GL_CLAMP_TO_EDGE);
	m_txGBufferDepth = Texture::Create2d(m_resolution.x, m_resolution.y, GL_DEPTH32F_STENCIL8);
	m_txGBufferDepth->setName("txGBufferDepth");
	m_txGBufferDepth->setFilter(GL_NEAREST);
	m_txGBufferDepth->setWrap(GL_CLAMP_TO_EDGE);
	m_fbGBuffer = Framebuffer::Create(4, m_txGBuffer0, m_txGBuffer1, m_txGBuffer2, m_txGBufferDepth);

	m_txScene = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA16F);
	m_txScene->setName("txScene");
	m_txScene->setFilter(GL_LINEAR);
	m_txScene->setWrap(GL_CLAMP_TO_EDGE);
	m_fbScene = Framebuffer::Create(2, m_txScene, m_txGBufferDepth);

	m_model = Model::Create("model/teapot.model.json");	

	m_worldMatrix = identity;

	return true;
}

void SceneGraph::shutdown()
{
	Texture::Release(m_txGBuffer0);
	Texture::Release(m_txGBuffer1);
	Texture::Release(m_txGBuffer2);
	Texture::Release(m_txGBufferDepth);
	Texture::Release(m_txScene);
	Framebuffer::Destroy(m_fbGBuffer);
	Framebuffer::Destroy(m_fbScene);

	Model::Release(m_model);

	AppBase::shutdown();
}

bool SceneGraph::update()
{
	if (!AppBase::update()) {
		return false;
	}

	// sample code here

	return true;
}

void SceneGraph::draw()
{
	auto ctx = GlContext::GetCurrent();

	auto cullCamera = Scene::GetCullCamera();
	auto drawCamera = Scene::GetDrawCamera();

	{	PROFILER_MARKER("gbuffer");
		ctx->setFramebufferAndViewport(m_fbGBuffer);
		//glAssert(glClear(GL_DEPTH_BUFFER_BIT));
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
		LodCoefficients lodCoefficients;
		lodCoefficients.m_distance = Length(GetTranslation(m_worldMatrix) - cullCamera->getPosition());
		lodCoefficients.m_size = lodCoefficients.m_distance / cullCamera->m_proj[1][1];
		int modelPass = m_model->findPass("gbuffer");
		if (modelPass >= 0) {
			int lod = m_model->findLod(modelPass, lodCoefficients);
			auto lodData = m_model->getLod(modelPass, lod);
ImGui::Text("Lod Size: %f, Lod: %d", lodCoefficients.m_size, lod);
			
			for (int i = 0; i < lodData.m_mesh->getSubmeshCount(); ++i) {
				if (lodData.m_materials[i]) {
					auto material = lodData.m_materials[i];
					
					int materialPass = material->findPass("gbuffer");
					if (materialPass >= 0) {
						auto& passData = material->getPass(materialPass);
						
						ctx->setShader(passData.m_shader);
						ctx->bindBuffer(drawCamera->m_gpuBuffer);
						ctx->setMesh(lodData.m_mesh, i);
						if (passData.m_state == "opaque") {
							glAssert(glEnable(GL_DEPTH_TEST));
							glAssert(glEnable(GL_CULL_FACE));
						} else if (passData.m_state == "shadow") {
							glAssert(glColorMask(0, 0, 0, 0));
							glAssert(glEnable(GL_DEPTH_TEST));
							glAssert(glEnable(GL_CULL_FACE));
						}
						for (auto tx : passData.m_textures) {
							ctx->bindTexture(tx.first.c_str(), tx.second);
						}
						ctx->setUniform("uWorld", m_worldMatrix);
						ctx->draw();
						if (passData.m_state == "opaque") {
							glAssert(glDisable(GL_DEPTH_TEST));
							glAssert(glDisable(GL_CULL_FACE));
						} else if (passData.m_state == "shadow") {
							glAssert(glColorMask(1, 1, 1, 1));
							glAssert(glDisable(GL_DEPTH_TEST));
							glAssert(glDisable(GL_CULL_FACE));
						}
					}
				}
			}
		}
	}
glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
	ctx->blitFramebuffer(m_fbGBuffer, nullptr);

	AppBase::draw();
}
