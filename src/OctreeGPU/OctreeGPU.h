#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class Octree;

class OctreeGPU: public AppBase
{
public:
	OctreeGPU();
	virtual ~OctreeGPU();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
	Octree*           m_octree         = nullptr;
	frm::vec3         m_octreeOrigin   = frm::vec3(0.0f);
	float             m_octreeWidth    = 32.0f;
	float             m_octreeHeight   = 8.0f;

	frm::Shader*      m_shPerVertex    = nullptr;
	frm::Mesh*        m_msPerVertex    = nullptr;
	frm::Shader*      m_shOctreeVis    = nullptr;
	frm::Texture*     m_txSceneColor   = nullptr;
	frm::Texture*     m_txSceneDepth   = nullptr;
	frm::Framebuffer* m_fbScene        = nullptr;
};
