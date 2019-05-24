#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class Voxelization: public AppBase
{
public:
	Voxelization();
	virtual ~Voxelization();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
	float             m_voxelsPerMeter          = 0.5f;
	frm::vec3         m_voxelVolumeSizeMeters   = frm::vec3(64.0f, 32.0f, 64.0f);
	frm::vec3         m_voxelVolumeOrigin       = frm::vec3(0.0f);
	frm::ivec3        m_voxelVolumeSizeVoxels   = frm::ivec3(0);
	frm::Buffer*      m_bfVoxelVolume           = nullptr;
	frm::Texture*     m_txVoxelVolume           = nullptr;
	frm::Shader*      m_shVoxelize              = nullptr;
	frm::Shader*      m_shCopyToVolume          = nullptr;

	frm::Mesh*        m_msTeapot                = nullptr;
	frm::mat4         m_worldMatrix             = frm::identity;
	frm::Shader*      m_shRasterize             = nullptr;

	frm::Shader*      m_shVolumeVis             = nullptr;
	frm::Mesh*        m_msVolumeVis             = nullptr;
	frm::Texture*     m_txSceneColor            = nullptr;
	frm::Texture*     m_txSceneDepth            = nullptr;
	frm::Framebuffer* m_fbScene                 = nullptr;

	bool initVoxelVolume();
	void shutdownVoxelVolume();
};
