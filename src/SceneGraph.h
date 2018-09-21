#pragma once

#include <frm/core/AppSample3d.h>

class Model;

class SceneGraph: public frm::AppSample3d
{
	typedef AppSample3d AppBase;
public:
	SceneGraph();
	virtual ~SceneGraph();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	Model*            m_model;
	frm::mat4         m_worldMatrix;

	frm::Texture*     m_txGBuffer0;
	frm::Texture*     m_txGBuffer1;
	frm::Texture*     m_txGBuffer2;
	frm::Texture*     m_txGBufferDepth;
	frm::Framebuffer* m_fbGBuffer;

	frm::Texture*     m_txScene;
	frm::Framebuffer* m_fbScene;
};

