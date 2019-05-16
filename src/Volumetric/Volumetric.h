#pragma once

#include <frm/core/AppSample3d.h>
#include <frm/core/XForm.h>

typedef frm::AppSample3d AppBase;

class Volumetric: public AppBase
{
public:
	Volumetric();
	virtual ~Volumetric();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
	struct VolumeData
	{
		frm::vec3 m_volumeOrigin     = frm::vec3(0.0f);
		float     m_volumeWidth      = 32.0f;//8000.0f;
		float     m_volumeHeight     = 4.0f;//1000.0f;

		float     m_coverageBias     = 0.0f;
		float     m_density          = 1.0f;
		float     m_scatter          = 1.0f;
		float     m_shapeScale       = 0.05f;
		float     m_erosionScale     = 10.0f; // relative to m_shapeScale
		float     m_erosionStrength  = 0.5f;		
	};
	VolumeData        m_volumeData;
	frm::Buffer*      m_bfVolumeData   = nullptr;
	frm::Texture*     m_txNoiseShape   = nullptr;
	frm::Texture*     m_txNoiseErosion = nullptr;
	frm::Texture*     m_txCloudControl = nullptr;

	bool editVolumeData();
	void updateVolumeDataGPU();

	frm::Shader*      m_shTest         = nullptr;

	frm::Texture*     m_txSceneColor   = nullptr;
	frm::Texture*     m_txSceneDepth   = nullptr;
	frm::Framebuffer* m_fbScene        = nullptr;

	frm::XForm_OrbitalPath m_sunPath;	
};
