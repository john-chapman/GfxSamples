#include "LensFlare_ScreenSpace.h"

#include <frm/core/frm.h>
#include <frm/core/gl.h>
#include <frm/core/ArgList.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Image.h>
#include <frm/core/Mesh.h>
#include <frm/core/MeshData.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>


using namespace frm;

static LensFlare_ScreenSpace s_inst;

LensFlare_ScreenSpace::LensFlare_ScreenSpace()
	: AppBase("LensFlare_ScreenSpace")
{
	Properties::PushGroup("LensFlare");
		//              name                       default                     min           max           storage
		Properties::Add("m_showLensFlareOnly",     m_showLensFlareOnly,                                    &m_showLensFlareOnly);
		Properties::Add("m_showFeaturesOnly",      m_showFeaturesOnly,                                     &m_showFeaturesOnly);
		Properties::Add("m_downsample",            m_downsample,               0,            4,            &m_downsample);
		Properties::Add("m_globalBrightness",      m_globalBrightness,         0.0f,         1.0f,         &m_globalBrightness);
		Properties::Add("m_chromaticAberration",   m_chromaticAberration,      0.0f,         0.2f,         &m_chromaticAberration);
		Properties::Add("m_ghostCount",            m_ghostCount,               0,            32,           &m_ghostCount);
		Properties::Add("m_ghostSpacing",          m_ghostSpacing,             0.0f,         2.0f,         &m_ghostSpacing);
		Properties::Add("m_ghostThreshold",        m_ghostThreshold,           0.0f,         20.0f,        &m_ghostThreshold);
		Properties::Add("m_haloRadius",            m_haloRadius,               0.0f,         2.0f,         &m_haloRadius);
		Properties::Add("m_haloThickness",         m_haloThickness,            0.0f,         0.5f,         &m_haloThickness);
		Properties::Add("m_haloThreshold",         m_haloThreshold,            0.0f,         20.0f,        &m_haloThreshold);
		Properties::Add("m_haloAspectRatio",       m_haloAspectRatio,          0.0f,         2.0f,         &m_haloAspectRatio);
		Properties::Add("m_blurSize",              m_blurSize,                 1,            64,           &m_blurSize);
		Properties::Add("m_blurStep",              m_blurStep,                 1.0f,         4.0f,         &m_blurStep);
	Properties::PopGroup();
}

LensFlare_ScreenSpace::~LensFlare_ScreenSpace()
{
	Properties::InvalidateGroup("LensFlare");
}

bool LensFlare_ScreenSpace::init(const ArgList& _args)
{
	if (!AppBase::init(_args)) {
		return false;
	}

	initScene();
	
	initLensFlare();	
	m_shDownsample = Shader::CreateCs("shaders/Downsample_cs.glsl", 8, 8);
	m_shFeatures = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/Features_fs.glsl");
	m_shBlur = Shader::CreateCs("shaders/GaussBlur_cs.glsl", 16, 16);	
	m_shComposite = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/Composite_fs.glsl");
	
	m_txGhostColorGradient = Texture::Create("textures/ghost_color_gradient.psd");
	m_txGhostColorGradient->setName("txGhostColorGradient");
	m_txGhostColorGradient->setWrap(GL_CLAMP_TO_EDGE);
	
	m_txLensDirt = Texture::Create("textures/lens_dirt.png");
	m_txLensDirt->setName("txLensDirt");
	m_txLensDirt->setWrap(GL_CLAMP_TO_EDGE);
	
	m_txStarburst = Texture::Create("textures/starburst.png");
	m_txStarburst->generateMipmap();
	m_txStarburst->setName("txStarburst");

	m_colorCorrection.init();

	return true;
}

void LensFlare_ScreenSpace::shutdown()
{
	Texture::Release(m_txGhostColorGradient);
	Texture::Release(m_txLensDirt);
	Texture::Release(m_txStarburst);

	shutdownScene();
	shutdownLensFlare();
	m_colorCorrection.shutdown();

	AppBase::shutdown();
}

bool LensFlare_ScreenSpace::update()
{
	if (!AppBase::update()) {
		return false;
	}

	bool reinit = false;
	ImGui::Begin("Lens Flare");
		if (ImGui::Checkbox("Lens Flare Only", &m_showLensFlareOnly)) {
			m_showFeaturesOnly = m_showLensFlareOnly ? false : m_showFeaturesOnly;
		}
		if (ImGui::Checkbox("Features Only", &m_showFeaturesOnly)) {
			m_showLensFlareOnly = m_showFeaturesOnly ? false : m_showLensFlareOnly;
		}
		reinit |= ImGui::SliderInt("Downsample", &m_downsample, 0, 4);
		ImGui::SliderFloat("Chromatic Aberration", &m_chromaticAberration, 0.0f, 0.2f);
		
		ImGui::Spacing();
		ImGui::SliderFloat("Global Brightness", &m_globalBrightness, 0.0f, 1.0f);
			
		ImGui::Spacing();
		ImGui::SliderInt("Ghost Count", &m_ghostCount, 0, 32);
		ImGui::SliderFloat("Ghost Spacing", &m_ghostSpacing, 0.0f, 2.0f);
		ImGui::SliderFloat("Ghost Threshold", &m_ghostThreshold, 0.0f, 20.0f);
		
		ImGui::Spacing();
		ImGui::SliderFloat("Halo Radius", &m_haloRadius, 0.0f, 2.0f);
		ImGui::SliderFloat("Halo Thickness", &m_haloThickness, 0.0f, 0.4f);
		ImGui::SliderFloat("Halo Threshold", &m_haloThreshold, 0.0f, 20.0f);
		ImGui::SliderFloat("Halo Aspect Ratio", &m_haloAspectRatio, 0.0f, 2.0f);

		//if (ImGui::TreeNode("Blur")) {
		//	ImGui::SliderInt("Blur Size", &m_blurSize, 4, 32);
		//	ImGui::SliderFloat("Blur Step", &m_blurStep, 1.0f, 4.0f);
		//	
		//	ImGui::TreePop();
		//}

		ImGui::Spacing();
		if (ImGui::TreeNode("Color Correction")) {
			m_colorCorrection.edit();
			ImGui::TreePop();
		}
	ImGui::End();
	if (reinit) {
		initLensFlare();
	}

	return true;
}

void LensFlare_ScreenSpace::draw()
{
	GlContext* ctx = GlContext::GetCurrent();
	Camera* cam = Scene::GetDrawCamera();

 // scene
	{	PROFILER_MARKER("Scene");
		ctx->setFramebufferAndViewport(m_fbScene);
		ctx->setShader(m_shEnvMap);
		ctx->bindTexture("txEnvmap", m_txEnvmap);
		ctx->drawNdcQuad(cam);
	}
	 
 // downsample
	{	PROFILER_MARKER("Downsample");
	
		const int localX = m_shDownsample->getLocalSize().x;
		const int localY = m_shDownsample->getLocalSize().y;
		int w = m_txSceneColor->getWidth() >> 1;
		int h = m_txSceneColor->getHeight() >> 1;
		int lvl = 0;
		m_txSceneColor->setMinFilter(GL_LINEAR_MIPMAP_NEAREST); // no filtering between mips
		while (w >= 1 && h >= 1) {
			ctx->setShader(m_shDownsample); // force reset bindings
			ctx->setUniform("uSrcLevel", lvl);
			ctx->bindTexture("txSrc", m_txSceneColor);
			ctx->bindImage("txDst", m_txSceneColor, GL_WRITE_ONLY, ++lvl);
			ctx->dispatch(
				Max((w + localX - 1) / localX, 1),
				Max((h + localY - 1) / localY, 1)
				);
			glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			w = w >> 1;
			h = h >> 1;
		}
		m_txSceneColor->setMinFilter(GL_LINEAR_MIPMAP_LINEAR);
	}

 // lens flare
	{	PROFILER_MARKER("Lens Flare");
		{	PROFILER_MARKER("Features");
			ctx->setFramebufferAndViewport(m_fbFeatures);
			ctx->setShader(m_shFeatures);
			ctx->bindTexture(m_txSceneColor);
			ctx->bindTexture(m_txGhostColorGradient);
			ctx->setUniform("uDownsample",          (float)m_downsample);
			ctx->setUniform("uGhostCount",          m_ghostCount);
			ctx->setUniform("uGhostSpacing",        m_ghostSpacing);
			ctx->setUniform("uGhostThreshold",      m_ghostThreshold);
			ctx->setUniform("uHaloRadius",          m_haloRadius);
			ctx->setUniform("uHaloThickness",       m_haloThickness);
			ctx->setUniform("uHaloThreshold",       m_haloThreshold);
			ctx->setUniform("uHaloAspectRatio",     m_haloAspectRatio);
			ctx->setUniform("uChromaticAberration", m_chromaticAberration);
			ctx->drawNdcQuad();
		}
		{	PROFILER_MARKER("Blur");
			ctx->setShader(m_shBlur);
			ctx->setUniform("uRadiusPixels", m_blurSize);

		 // horizontal
			ctx->setUniform("uDirection", vec2(m_blurStep, 0.0f));
			ctx->bindTexture("txSrc", m_txFeatures[0]);
			ctx->bindImage("txDst", m_txFeatures[1], GL_WRITE_ONLY);
			ctx->dispatch(m_txFeatures[1]);
			glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			
			ctx->clearImageBindings();
			ctx->clearTextureBindings();
			
		 // vertical
			ctx->setUniform("uDirection", vec2(0.0f, m_blurStep));
			ctx->bindTexture("txSrc", m_txFeatures[1]);
			ctx->bindImage("txDst", m_txFeatures[0], GL_WRITE_ONLY);
			ctx->dispatch(m_txFeatures[0]);
			glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
		}
		{	PROFILER_MARKER("Composite");

			vec3 viewVec = Scene::GetDrawCamera()->getViewVector();
			float starburstOffset = viewVec.x + viewVec.y + viewVec.z;

			ctx->setFramebufferAndViewport(m_fbScene);
			if (m_showLensFlareOnly) {
				glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
				glAssert(glClear(GL_COLOR_BUFFER_BIT));
			}
			ctx->setShader(m_shComposite);
			ctx->setUniform("uGlobalBrightness", m_globalBrightness);
			ctx->setUniform("uStarburstOffset",  starburstOffset);
			ctx->bindTexture(m_txFeatures[0]);
			ctx->bindTexture(m_txLensDirt);
			ctx->bindTexture(m_txStarburst);
			glAssert(glEnable(GL_BLEND));
			glAssert(glBlendFunc(GL_ONE, GL_ONE));
			ctx->drawNdcQuad();
			glAssert(glDisable(GL_BLEND));
		}
	}
	
	if (m_showFeaturesOnly) {
		m_colorCorrection.draw(ctx, m_txFeatures[0], nullptr);
	} else {
		m_colorCorrection.draw(ctx, m_txSceneColor, nullptr);
	}
	
	AppBase::draw();
}

bool LensFlare_ScreenSpace::initScene()
{
	shutdownScene();

	m_txSceneColor = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA16F, 99);
	m_txSceneColor->setName("txSceneColor");
	m_txSceneColor->setWrap(GL_CLAMP_TO_EDGE);
	m_txSceneColor->setMinFilter(GL_LINEAR_MIPMAP_NEAREST);
	m_txSceneDepth = Texture::Create2d(m_resolution.x, m_resolution.y, GL_DEPTH32F_STENCIL8);
	m_txSceneDepth->setName("txSceneDepth");
	m_txSceneDepth->setWrap(GL_CLAMP_TO_EDGE);
	m_fbScene = Framebuffer::Create(2, m_txSceneColor, m_txSceneDepth);

	m_txEnvmap = Texture::Create("textures/env_factory.dds");
	m_shEnvMap = Shader::CreateVsFs("shaders/Envmap_vs.glsl", "shaders/Envmap_fs.glsl", { "ENVMAP_CUBE" });
	
	return true;
}

void LensFlare_ScreenSpace::shutdownScene()
{
	Texture::Release(m_txSceneColor);
	Texture::Release(m_txSceneDepth);
	Framebuffer::Destroy(m_fbScene);
	Texture::Release(m_txEnvmap);
	Shader::Release(m_shEnvMap);
}


bool LensFlare_ScreenSpace::initLensFlare()
{
	shutdownLensFlare();

	ivec2 sz = ivec2(m_txSceneColor->getWidth(), m_txSceneColor->getHeight());
	sz.x = Max(sz.x >> m_downsample, 1);
	sz.y = Max(sz.y >> m_downsample, 1);
	m_txFeatures[0] = Texture::Create2d(sz.x, sz.y, m_txSceneColor->getFormat());
	m_txFeatures[0]->setName("txFeatures");
	m_txFeatures[0]->setWrap(GL_CLAMP_TO_EDGE);
	m_txFeatures[1] = Texture::Create(m_txFeatures[0], false);
	m_txFeatures[1]->setWrap(GL_CLAMP_TO_EDGE);
	m_fbFeatures = Framebuffer::Create(1, m_txFeatures[0]);

	return true;
}

void LensFlare_ScreenSpace::shutdownLensFlare()
{
	Texture::Release(m_txFeatures[0]);
	Texture::Release(m_txFeatures[1]);
	Framebuffer::Destroy(m_fbFeatures);
}
