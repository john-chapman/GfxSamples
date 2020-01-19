#pragma once

#include <frm/core/AppSample.h>
#include <frm/core/Texture.h>

typedef frm::AppSample AppBase;

class Convolution: public frm::AppSample
{
	typedef AppSample AppBase;
public:
	Convolution();
	virtual ~Convolution();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
	enum Type_
	{
		Type_Box,
		Type_Gaussian,
		Type_Binomial,

		Type_Count
	};
	typedef int Type;

	enum Mode_
	{
		Mode_2d,
		Mode_2dBilinear,
		Mode_Separable,
		Mode_SeparableBilinear,
		Mode_Prefilter,

		Mode_Count
	};
	typedef int Mode;

	Type   m_kernelType              = Type_Box;
	Mode   m_kernelMode              = Mode_Separable;
	int    m_kernelWidth             = 7;
	int    m_kernelSize              = m_kernelWidth * m_kernelWidth;
	float  m_gaussianSigma           = 1.0f;
	float  m_gaussianSigmaOptimal    = 1.0f;
	float  m_kernelSum               = 0.0f;
	float  m_prefilterLodBias        = 1.0f;
	int    m_prefilterSampleCount    = 8;
	int    m_prefilterBlurWidth      = 21;
	float* m_weights                 = nullptr;
	float* m_offsets                 = nullptr;
	float* m_displayWeights          = nullptr;
	bool   m_showKernel              = false;
	bool   m_cached                  = false;

	void initKernel();
	void shutdownKernel();

	void initOffsets();
	void initWeights();

	void copyWeightsToClipboard();
	void copyOffsetstoClipboard();

	frm::Texture*    m_txSrc                     = nullptr;
	frm::Texture*    m_txDst[2]                  = { nullptr };
	frm::TextureView m_txDstView;
	frm::Shader*     m_shConvolutionBasic        = nullptr;
	frm::Shader*     m_shConvolutionCached[2]    = { nullptr };
	frm::Shader*     m_shPrefilter               = nullptr;
	frm::Shader*     m_shConvolutionPrefiltered  = nullptr;
	frm::Buffer*     m_bfWeights                 = nullptr;
	frm::Buffer*     m_bfOffsets                 = nullptr;
};
