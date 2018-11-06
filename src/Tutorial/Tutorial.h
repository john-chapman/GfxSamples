#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class Tutorial: public AppBase
{
public:
	Tutorial();
	virtual ~Tutorial();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
	frm::Texture*     m_txScene         = nullptr;
	frm::Texture*     m_txSceneDepth    = nullptr;
	frm::Framebuffer* m_fbScene         = nullptr;
	frm::Texture*     m_txDiffuse       = nullptr;
	frm::Shader*      m_shMesh          = nullptr;
	frm::Mesh*        m_mesh            = nullptr;
	frm::mat4         m_worldMatrix     = frm::identity;
	float             m_scale           = 1.0f;
	frm::Shader*      m_shPostProcess   = nullptr;
};
