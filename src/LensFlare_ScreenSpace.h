#pragma once

#include <frm/core/AppSample3d.h>
#include <frm/core/RenderNodes.h>

class LensFlare_ScreenSpace: public frm::AppSample3d
{
	typedef AppSample3d AppBase;
public:
	LensFlare_ScreenSpace();
	virtual ~LensFlare_ScreenSpace();

	virtual bool init(const apt::ArgList& _args) override;
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
	bool                m_showLensFlareOnly;
	bool                m_showFeaturesOnly;
	int                 m_downsample;
	int                 m_ghostCount;
	float               m_ghostSpacing;
	float               m_ghostThreshold;
	frm::Texture*       m_txGhostColorGradient;
	float               m_haloRadius;
	float               m_haloThickness;
	float               m_haloThreshold;
	float               m_haloAspectRatio;
	float               m_chromaticAberration;
	int                 m_blurSize;
	float               m_blurStep;
	float               m_globalBrightness;
	frm::Texture*       m_txLensDirt;
	frm::Texture*       m_txStarburst;
	frm::Shader*        m_shDownsample;
	frm::Shader*        m_shFeatures;
	frm::Shader*        m_shBlur;
	frm::Shader*        m_shComposite;
	frm::Texture*       m_txFeatures[2]; // 2 targets for separable blur
	frm::Framebuffer*   m_fbFeatures;

	bool initLensFlare();
	void shutdownLensFlare();
};
