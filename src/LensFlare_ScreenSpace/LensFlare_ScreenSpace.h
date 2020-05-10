#pragma once

#include <frm/core/AppSample3d.h>
#include <frm/core/RenderNodes.h>

class LensFlare_ScreenSpace: public frm::AppSample3d
{
	typedef AppSample3d AppBase;
public:
	LensFlare_ScreenSpace();
	virtual ~LensFlare_ScreenSpace();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

 // render nodes
	frm::ColorCorrection m_colorCorrection;

 // scene
	frm::Texture*     m_txSceneColor;
	frm::Texture*     m_txSceneDepth;
	frm::Framebuffer* m_fbScene;
	frm::Shader*      m_shEnvMap;
	frm::Texture*     m_txEnvmap;
	frm::Shader*      m_shColorCorrection;
	
	bool initScene();
	void shutdownScene();


 // lens flare
	bool                m_showLensFlareOnly        = false;
	bool                m_showFeaturesOnly         = false;
	int                 m_downsample               = 2;
	int                 m_ghostCount               = 8;
	float               m_ghostSpacing             = 0.3f;
	float               m_ghostThreshold           = 12.0f;
	float               m_haloRadius               = 0.5f;
	float               m_haloThickness            = 0.05f;
	float               m_haloThreshold            = 20.0f;
	float               m_haloAspectRatio          = 0.6f;
	float               m_chromaticAberration      = 0.002f;
	int                 m_blurSize                 = 8;
	float               m_blurStep                 = 4.0f;
	float               m_globalBrightness         = 0.2f;
	frm::Texture*       m_txGhostColorGradient     = nullptr;
	frm::Texture*       m_txLensDirt               = nullptr;
	frm::Texture*       m_txStarburst              = nullptr;
	frm::Shader*        m_shDownsample             = nullptr;
	frm::Shader*        m_shFeatures               = nullptr;
	frm::Shader*        m_shBlur                   = nullptr;
	frm::Shader*        m_shComposite              = nullptr;
	frm::Texture*       m_txFeatures[2]            = { nullptr }; // 2 targets for separable blur
	frm::Framebuffer*   m_fbFeatures               = nullptr;

	bool initLensFlare();
	void shutdownLensFlare();
};
