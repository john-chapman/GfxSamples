#include "Convolution.h"

#include <frm/core/frm.h>
#include <frm/core/rand.h>
#include <frm/core/ArgList.h>
#include <frm/core/Buffer.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>
#include <frm/core/Window.h>

using namespace frm;

namespace {

#define GAUSSIAN_USE_INTEGRATION 1

inline float GaussianDistribution(float _d, float _sigma, float _sigma2)
{
	float d = (_d * _d) / (2.0f * _sigma2);
	return 1.0f / (kTwoPi * _sigma2) * exp(-d);
}

// Evaluate GaussianDistribution() over the interval [_mind,_maxd] via trapezoidal integration.
float GaussianIntegration(float _mind, float _maxd, float _sigma, float _sigma2, int _sampleCount = 64)
{
	const float w = 1.0f / (float)(_sampleCount - 1);
	const float stepd = (_maxd - _mind) * w;
	float ret = GaussianDistribution(_mind, _sigma, _sigma2);
	float d = _mind + stepd;
	for (int i = 1; i < _sampleCount - 1; ++i) 
	{
		ret += GaussianDistribution(d, _sigma, _sigma2) * 2.0f;
		d += stepd;
	}
	ret += GaussianDistribution(d, _sigma, _sigma2);
	return ret * stepd / 2.0f;
}

float KernelGaussian1d(int _size, float _sigma, float* weights_, bool _normalize = true)
{
	_size = _size | 1; // force _size to be odd

 // generate
	const float sigma2 = _sigma * _sigma;
	const int n = _size / 2;
	float sum = 0.0f;
	for (int i = 0; i < _size; ++i) 
	{
		float d = (float)(i - n);
		#if GAUSSIAN_USE_INTEGRATION
			weights_[i] = GaussianIntegration(d - 0.5f, d + 0.5f, _sigma, sigma2);
		#else
			weights_[i] = GaussianDistribution(d, _sigma, sigma2);
		#endif
		sum += weights_[i];
	}

 // normalize
	if (_normalize) 
	{
		for (int i = 0; i < _size; ++i) 
		{
			weights_[i] /= sum;
		}
	}
	return sum;
}

float KernelGaussian2d(int _size, float _sigma, float* weights_, bool _normalize = true)
{
	_size = _size | 1; // force _size to be odd

 // generate first row
	float sum = KernelGaussian1d(_size, _sigma, weights_, false); // not normalized because we overwrite the first row later
	sum *= sum;	

 // derive subsequent rows from the first
	for (int i = 1; i < _size; ++i) 
	{
		for (int j = 0; j < _size; ++j) 
		{
			int k = i * _size + j;
			weights_[k] = (weights_[i] * weights_[j]) / (_normalize ? sum : 1.0f);
		}
	}

 // copy the first row from the last
	for (int i = 0; i < _size; ++i) 
	{
		weights_[i] = weights_[(_size - 1) * _size + i];
	}
	return sum;
}

// Find sigma such that no weights are < _epsilon. Epsilon should be the smallest representable for the precision of the signal to be convolved e.g. 1/255 for 8-bit.
float GaussianFindSigma(int _size, float _epsilon)
{
	float* tmp = FRM_NEW_ARRAY(float, _size);
	const float d = (float)(-_size / 2);
	float sigma = 1.0f;
	float stp = 1.0f;
	while (stp > 0.01f) 
	{
		KernelGaussian1d(_size, sigma, tmp);
		float w = tmp[0];
		if (w > _epsilon) 
		{
			sigma -= stp;
			stp *= 0.5f;
		}
		sigma += stp;
	}
	FRM_DELETE_ARRAY(tmp);
	return sigma;
}

float KernelBinomial1d(int _size, float* weights_, bool _normalize = true)
{
	_size = _size | 1; // force _size to be odd

 // generate (Pascal's triangle)
	weights_[0] = 1.0f;
	float sum = 1.0f;
	for (int i = 1; i < _size; ++i) 
	{
		weights_[i] = weights_[i - 1] * (float)(_size - i) / (float)i;
		sum += weights_[i];
	}

 // normalize
	if (_normalize) 
	{
		for (int i = 0; i < _size; ++i) 
		{
			weights_[i] /= sum;
		}
	}
	return sum;
}

float KernelBinomial2d(int _size, float* weights_, bool _normalize = true)
{
	_size = _size | 1; // force _size to be odd

 // generate first row
	float sum = KernelBinomial1d(_size, weights_, false); // not normalized because we overwrite the first row later
	sum *= sum;	

 // derive subsequent rows from the first
	for (int i = 1; i < _size; ++i) 
	{
		for (int j = 0; j < _size; ++j) 
		{
			int k = i * _size + j;
			weights_[k] = (weights_[i] * weights_[j]) / (_normalize ? sum : 1.0f);
		}
	}

 // copy the first row from the last
	for (int i = 0; i < _size; ++i) 
	{
		weights_[i] = weights_[(_size - 1) * _size + i];
	}
	return sum;
}

// Outputs are arrays of _size / 2 + 1.
void KernelOptimizeBilinear1d(int _size, const float* _weightsIn, float* weightsOut_, float* offsetsOut_)
{
	const int halfSize = _size / 2;
	int j = 0;
	for (int i = 0; i != _size - 1; i += 2, ++j) 
	{
		float w1 = _weightsIn[i];
		float w2 = _weightsIn[i + 1];
		float w3 = w1 + w2;
		float o1 = (float)(i - halfSize);
		float o2 = (float)(i - halfSize + 1);
		float o3 = (o1 * w1 + o2 * w2) / w3;
		weightsOut_[j] = w3;
		offsetsOut_[j] = o3;
	}
	weightsOut_[j] = _weightsIn[_size - 1];
	offsetsOut_[j] = (float)(_size - 1 - halfSize);
}

// Outputs are arrays of (_size / 2 + 1) ^ 2.
void KernelOptimizeBilinear2d(int _size, const float* _weightsIn, float* weightsOut_, vec2* offsetsOut_)
{
	const int outSize = _size / 2 + 1;
	const int halfSize = _size / 2;
	int row, col;
	for (row = 0; row < _size - 1; row += 2) 
	{
		for (col = 0; col < _size - 1; col += 2) 
		{
			float w1 = _weightsIn[(row * _size) + col];
			float w2 = _weightsIn[(row * _size) + col + 1];
			float w3 = _weightsIn[((row + 1) * _size) + col];
			float w4 = _weightsIn[((row + 1) * _size) + col + 1];
			float w5 = w1 + w2 + w3 + w4;
			float x1 = (float)(col - halfSize);
			float x2 = (float)(col - halfSize + 1);
			float x3 = (x1 * w1 + x2 * w2) / (w1 + w2);
			float y1 = (float)(row - halfSize);
			float y2 = (float)(row - halfSize + 1);
			float y3 = (y1 * w1 + y2 * w3) / (w1 + w3);

			const int k = (row / 2) * outSize + (col / 2);
			weightsOut_[k] = w5;
			offsetsOut_[k] = vec2(x3, y3);
		}

		float w1 = _weightsIn[(row * _size) + col];
		float w2 = _weightsIn[((row + 1) * _size) + col];
		float w3 = w1 + w2;
		float y1 = (float)(row - halfSize);
		float y2 = (float)(row - halfSize + 1);
		float y3 = (y1 * w1 + y2 * w2) / w3;
	
		const int k = (row / 2) * outSize + (col / 2);
		weightsOut_[k] = w3;
		offsetsOut_[k] = vec2((float)(col - halfSize), y3);
	}

	for (col = 0; col < _size - 1; col += 2) 
	{
		float w1 = _weightsIn[(row * _size) + col];
		float w2 = _weightsIn[(row * _size) + col + 1];
		float w3 = w1 + w2;
		float x1 = (float)(col - halfSize);
		float x2 = (float)(col - halfSize + 1);
		float x3 = (x1 * w1 + x2 * w2) / w3;

		const int k = (row / 2) * outSize + (col / 2);
		weightsOut_[k] = w3;
		offsetsOut_[k] = vec2(x3, (float)(row - halfSize));
	}

	const int k = (row / 2) * outSize + (col / 2);
	weightsOut_[k] = _weightsIn[(row * _size) + col];
	offsetsOut_[k] = vec2(_size / 2.0f);	
}

} // namespace

static Convolution s_inst;

Convolution::Convolution()
	: AppBase("Convolution")
{
	PropertyGroup& propGroup = m_props.addGroup("Convolution");
	//                  name                       default                  min     max          storage
	propGroup.addInt   ("m_kernelType",            m_kernelType,            0,      Type_Count,  &m_kernelType);
	propGroup.addInt   ("m_kernelMode",            m_kernelMode,            0,      Mode_Count,  &m_kernelMode);
	propGroup.addInt   ("m_kernelWidth",           m_kernelWidth,           1,      21,          &m_kernelWidth);
	propGroup.addFloat ("m_gaussianSigma",         m_gaussianSigma,         0.0f,   4.0f,        &m_gaussianSigma);
	propGroup.addFloat ("m_prefilterLodBias",      m_prefilterLodBias,      0.0f,   2.0f,        &m_prefilterLodBias);
	propGroup.addInt   ("m_prefilterSampleCount",  m_prefilterSampleCount,  2,      64,          &m_prefilterSampleCount);
	propGroup.addInt   ("m_prefilterBlurWidth",    m_prefilterBlurWidth,    0,      64,          &m_prefilterBlurWidth);
	propGroup.addBool  ("m_cached",                m_cached,                                     &m_cached);
	propGroup.addBool  ("m_showKernel",            m_showKernel,                                 &m_showKernel);
}

Convolution::~Convolution()
{
}

bool Convolution::init(const ArgList& _args)
{
	if (!AppBase::init(_args)) 
	{
		return false;
	}

	m_txSrc = Texture::Create("textures/baboon.png");
	m_txSrc->setWrap(GL_CLAMP_TO_EDGE);
	m_txSrc->generateMipmap(); // alloc mip chain for Mode_Prefilter
	
	for (uint i = 0; i < FRM_ARRAY_COUNT(m_txDst); ++i) 
	{
		m_txDst[i] = Texture::Create2d(m_txSrc->getWidth(), m_txSrc->getHeight(), GL_RGBA8, 99);
		m_txDst[i]->setWrap(GL_CLAMP_TO_EDGE);
		m_txDst[i]->setNamef("txDst[%d]", i);
	}
	m_txDstView = TextureView(m_txDst[0]);

	m_shPrefilter = Shader::CreateCs("shaders/Prefilter_cs.glsl", 8, 8);
	m_shConvolutionPrefiltered = Shader::CreateCs("shaders/ConvolutionPrefiltered_cs.glsl", 8, 8);

	initKernel();

	return true;
}

void Convolution::shutdown()
{
	shutdownKernel();

	Shader::Release(m_shPrefilter);

	Texture::Release(m_txDst[0]);
	Texture::Release(m_txDst[1]);
	Texture::Release(m_txSrc);

	AppBase::shutdown();
}

bool Convolution::update()
{
	if (!AppBase::update()) 
	{
		return false;
	}

	bool reinitKernel = false;

	const vec2 borderSize   = vec2(32.0f);
	const vec2 previewSize  = vec2(m_txDstView.m_texture->getWidth(), m_txDstView.m_texture->getHeight());

	ImGui::BeginInvisible("preview", borderSize, previewSize);
	{
		auto drawList = ImGui::GetWindowDrawList();
		drawList->AddImage((ImTextureID)&m_txDstView, borderSize, borderSize + previewSize);
	}
	ImGui::EndInvisible();

	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
	ImGui::SetNextWindowPos(vec2(borderSize.x + previewSize.x, borderSize.y));
	ImGui::SetNextWindowSize(vec2(m_windowSize.x - previewSize.x - borderSize.x, (float)m_windowSize.y));
	ImGui::Begin("main", nullptr, 0
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		);
	{
		
		reinitKernel |= ImGui::Combo("Mode", &m_kernelMode,
			"2d\0"
			"2d Bilinear\0"
			"Seperable\0"
			"Seperable Bilinear\0"
			"Prefilter\0"
			);
		
		if (m_kernelMode == Mode_Prefilter)
		{
			
			ImGui::SliderInt("Blur Width", &m_prefilterBlurWidth, 0, 64);
			ImGui::SliderFloat("LOD Bias", &m_prefilterLodBias, 0.0f, 2.0f);
			ImGui::SliderInt("Sample Count", &m_prefilterSampleCount, 1, 16);
		}
		else
		{
			
			reinitKernel |= ImGui::Combo("Type", &m_kernelType,
				"Box\0"
				"Gaussian\0"
				"Binomial\0"
				);

			if (m_kernelMode == Mode_Separable)
			{
				ImGui::Checkbox("Cache Texture Reads", &m_cached);
				if (m_cached)
				{
					auto localSize = m_shConvolutionCached[0]->getLocalSize();
					if (ImGui::InputInt2("Shader Local Size", &localSize.x))
					{
						m_shConvolutionCached[0]->setLocalSize(localSize.x, localSize.y, localSize.z);
						m_shConvolutionCached[1]->setLocalSize(localSize.x, localSize.y, localSize.z);
					}
				}
			}
			reinitKernel |= ImGui::SliderInt("Size", &m_kernelWidth, 3, 21);
			if (m_kernelType == Type_Gaussian) 
			{
				reinitKernel |= ImGui::SliderFloat("Sigma", &m_gaussianSigma, 1.0f, 8.0f);
				ImGui::Text("Optimal Sigma: %f", m_gaussianSigmaOptimal);
			}
		}

		if (reinitKernel) 
		{
			initKernel();
		}

		if (m_kernelMode != Mode_Prefilter)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			vec2 graphBeg = vec2(ImGui::GetCursorPos()) + vec2(ImGui::GetWindowPos());
			vec2 grapkernelHalfWidth = vec2(ImGui::GetContentRegionAvailWidth() - ImGui::GetContentRegionAvailWidth() / 3, 300.0f);
			vec2 graphEnd = graphBeg + grapkernelHalfWidth;
			drawList->AddRectFilled(graphBeg, graphEnd, IM_COL32(39, 38, 54, 255));
			ImGui::InvisibleButton("kernel", graphEnd - graphBeg);
			ImGui::PushClipRect(graphBeg, graphEnd, true);
			
			float weightsScale = 0.0f;
			for (int i = 0; i < m_kernelWidth; ++i) 
			{
				weightsScale = FRM_MAX(weightsScale, m_displayWeights[i]);
			}
			weightsScale = weightsScale * 1.1f;

		 // sample weights
			for (int i = 0; i < m_kernelWidth; ++i)
			{
				float x0 = floor(graphBeg.x + (float)i / (float)m_kernelWidth * grapkernelHalfWidth.x);
				float x1 = floor(graphBeg.x + (float)(i + 1) / (float)m_kernelWidth * grapkernelHalfWidth.x);
				float y = (1.0f - m_displayWeights[i] / weightsScale) * grapkernelHalfWidth.y;
				drawList->AddRectFilled(vec2(x0, graphEnd.y), vec2(x1, graphBeg.y + y), IM_COLOR_ALPHA(IM_COL32_WHITE, 0.2f));
			}

		 // gaussian function
			if (m_kernelType == Type_Gaussian) 
			{

				const int directSampleCount = (int)grapkernelHalfWidth.x / 4;
				vec2 q = graphBeg + vec2(0.0f, grapkernelHalfWidth.y);
				for (int i = 0; i < directSampleCount; ++i) 
				{
					vec2 p;
					p.x = (float)i / (float)directSampleCount;
					float d = p.x * (float)m_kernelWidth - (float)m_kernelWidth * 0.5f;
					p.y = GaussianDistribution(d, m_gaussianSigma, m_gaussianSigma * m_gaussianSigma) / m_kernelSum / weightsScale;
					p.x = graphBeg.x + p.x * grapkernelHalfWidth.x;
					p.y = graphEnd.y - p.y * grapkernelHalfWidth.y;
					drawList->AddLine(q, p, IM_COL32(255, 200, 11, 255), 3.0f);
					q = p;
				}
			}

		 // texel boundaries
			for (int i = 1; i < m_kernelWidth; ++i) 
			{
				float x = floor(graphBeg.x + (float)i / (float)m_kernelWidth * grapkernelHalfWidth.x);
				drawList->AddLine(vec2(x, graphBeg.y), vec2(x, graphEnd.y), ImColor(0.5f, 0.5f, 0.5f));
			}
			drawList->AddRect(graphBeg, graphEnd, ImColor(0.5f, 0.5f, 0.5f));

			ImGui::PopClipRect();
	
	
			ImGui::Spacing();
			if (ImGui::Button(ICON_FA_CLIPBOARD " Copy Weights")) 
			{
				copyWeightsToClipboard();
			}
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_CLIPBOARD " Copy Offsets")) 
			{
				copyOffsetstoClipboard();
			}
		}
	}
	ImGui::End(); 
	ImGui::PopStyleColor();

	return true;
}

void Convolution::draw()
{
	GlContext* ctx = GlContext::GetCurrent();

	bool is2d = m_kernelMode == Mode_2d || m_kernelMode == Mode_2dBilinear;
	
	{	PROFILER_MARKER("Convolution");

		if (m_kernelMode == Mode_Separable && m_cached)
		{
			ctx->setShader  (m_shConvolutionCached[0]);
			ctx->bindBuffer (m_bfWeights);
			ctx->bindTexture("txSrc", m_txSrc);
			ctx->bindImage  ("txDst", m_txDst[1], GL_WRITE_ONLY);
			ctx->dispatch   (m_txDst[1]);
			glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
			ctx->setShader  (m_shConvolutionCached[1]);
			ctx->bindBuffer (m_bfWeights);
			ctx->bindTexture("txSrc", m_txDst[1]);
			ctx->bindImage  ("txDst", m_txDst[0], GL_WRITE_ONLY);

		 // note that we swizzle the texture coords in the shader to blur vertically, hence we need to manually compute the group count			
			//ctx->dispatch   (m_txDst[0]);
			auto localSize = m_shConvolutionCached[1]->getLocalSize();
			auto txSize    = ivec2(m_txDst[0]->getHeight(), m_txDst[0]->getWidth()); // swap width/height
			ctx->dispatch(
				FRM_MAX((txSize.x + localSize.x - 1) / localSize.x, 1),
				FRM_MAX((txSize.y + localSize.y - 1) / localSize.y, 1)
				);
		}
		else if (m_kernelMode == Mode_Prefilter)
		{
			float radius = m_prefilterBlurWidth * 0.5f;
			float area   = kPi * (radius * radius);
				  area   = area / (float)m_prefilterSampleCount; // area per sample
			float lod    = log2(sqrt(area)) + m_prefilterLodBias; // select mip level with similar area to the sample
			int   maxLod = Clamp((int)ceil(lod) + 1, 0, (int)m_txDst[0]->getMipCount() - 1);
		 // \todo can't downsample directly on m_txSrc?
			{	PROFILER_MARKER("Prefilter");
				auto txDst = m_txDst[1];
				txDst->setMinFilter(GL_LINEAR_MIPMAP_NEAREST); // no filtering between mips
				ivec2 localSize = m_shPrefilter->getLocalSize().xy();
				ctx->setShader(m_shPrefilter);
				for (int level = 0; level <= maxLod; ++level)
				{ 
					ctx->clearTextureBindings();
					ctx->clearImageBindings();
					ctx->setUniform ("uSrcLevel", level - 1);
					ctx->bindTexture("txSrc", level == 0 ? m_txSrc : txDst);
					ctx->bindImage  ("txDst", txDst, GL_WRITE_ONLY, level);

					int w = (int)txDst->getWidth() >> level;
					int h = (int)txDst->getHeight() >> level;
					ctx->dispatch(
						Max((w + localSize.x - 1) / localSize.x, 1),
						Max((h + localSize.y - 1) / localSize.y, 1)
						);

					glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
				}
				txDst->setMinFilter(GL_LINEAR_MIPMAP_LINEAR);
				txDst->setMipRange(0, maxLod);
			}
			{	PROFILER_MARKER("Blur");

				ctx->setShader  (m_shConvolutionPrefiltered);
				ctx->setUniform ("uRadius", radius);
				ctx->setUniform ("uLod", lod);
				ctx->setUniform ("uSampleCount", m_prefilterSampleCount);
				ctx->bindTexture("txSrc", m_txDst[1]);
				ctx->bindImage  ("txDst", m_txDst[0], GL_WRITE_ONLY);
				ctx->dispatch   (m_txDst[0]);
			}
		}
		else
		{
			ctx->setShader (m_shConvolutionBasic);
			ctx->bindBuffer(m_bfOffsets);
			ctx->bindBuffer(m_bfWeights);

			if (is2d) 
			{
				ctx->bindTexture("txSrc", m_txSrc);
				ctx->bindImage  ("txDst", m_txDst[0], GL_WRITE_ONLY);
				ctx->dispatch   (m_txDst[0]);

			} 
			else 
			{
				ctx->bindTexture("txSrc", m_txSrc);
				ctx->bindImage  ("txDst", m_txDst[1], GL_WRITE_ONLY);
				ctx->setUniform	("uDirection", vec2(1.0f, 0.0f));
				ctx->dispatch   (m_txDst[1]);
				glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
				ctx->clearTextureBindings();
				ctx->clearImageBindings();
				ctx->bindTexture("txSrc", m_txDst[1]);
				ctx->bindImage  ("txDst", m_txDst[0], GL_WRITE_ONLY);
				ctx->setUniform	("uDirection", vec2(0.0f, 1.0f));
				ctx->dispatch   (m_txDst[0]);

			}
		}
	}

	AppBase::draw();
}

void Convolution::initKernel()
{
	shutdownKernel();

	initOffsets();
	initWeights();

 // buffers
	const bool is2d = m_kernelMode == Mode_2d || m_kernelMode == Mode_2dBilinear;
	m_bfWeights = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(float) * m_kernelSize, 0, m_weights);
	m_bfWeights->setName("bfWeights");
	m_bfOffsets = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(float) * m_kernelSize * (is2d ? 2 : 1), 0, m_offsets); // offsets are vec2 for a 2d kernel
	m_bfOffsets->setName("bfOffsets");
	
 // shaders
	ShaderDesc shDesc;
	shDesc.setPath(GL_COMPUTE_SHADER, "shaders/ConvolutionBasic_cs.glsl");
	shDesc.setLocalSize(8, 8);
	shDesc.addDefine(GL_COMPUTE_SHADER, "TYPE", m_kernelType);
	shDesc.addDefine(GL_COMPUTE_SHADER, "MODE", m_kernelMode);
	shDesc.addDefine(GL_COMPUTE_SHADER, "KERNEL_SIZE", m_kernelSize);
	m_shConvolutionBasic = Shader::Create(shDesc);

	shDesc.setPath(GL_COMPUTE_SHADER, "shaders/ConvolutionCached_cs.glsl");
	shDesc.setLocalSize(64, 1); // local X must be *at least* the kernel radius to have enough threads to fill the cache 
	shDesc.addDefine(GL_COMPUTE_SHADER, "MODE", (int)Mode_Separable);
	shDesc.addDefine(GL_COMPUTE_SHADER, "DIMENSION", 0);
	m_shConvolutionCached[0] = Shader::Create(shDesc);
	shDesc.addDefine(GL_COMPUTE_SHADER, "DIMENSION", 1);
	m_shConvolutionCached[1] = Shader::Create(shDesc);
}

void Convolution::shutdownKernel()
{
	FRM_DELETE_ARRAY(m_weights);
	FRM_DELETE_ARRAY(m_displayWeights);
	FRM_DELETE_ARRAY(m_offsets);

	Shader::Release(m_shConvolutionCached[0]);
	Shader::Release(m_shConvolutionCached[1]);
	Shader::Release(m_shConvolutionBasic);
	Buffer::Destroy(m_bfWeights);
	Buffer::Destroy(m_bfOffsets);
}

void Convolution::initOffsets()
{
	m_kernelWidth = m_kernelWidth | 1; // force size to be odd
	const bool is2d = m_kernelMode == Mode_2d || m_kernelMode == Mode_2dBilinear;
	const int kernelHalfWidth = m_kernelWidth / 2;
	m_kernelSize = m_kernelWidth * (is2d ? m_kernelWidth : 1);

	m_offsets = FRM_NEW_ARRAY(float, m_kernelSize * (is2d ? 2 : 1)); // offsets are vec2 for a 2d kernel
	if (is2d) 
	{
		for (int i = 0; i < m_kernelWidth; ++i) 
		{
			float y = (float)(i - kernelHalfWidth);
			for (int j = 0; j < m_kernelWidth; ++j) 
			{
				float x = (float)(j - kernelHalfWidth);
				int k = (i * m_kernelWidth + j);
				m_offsets[k * 2] = x;
				m_offsets[k * 2 + 1] = y;
			}
		}
	} 
	else 
	{
		for (int i = 0; i < m_kernelWidth; ++i) 
		{
			m_offsets[i] = (float)(i - kernelHalfWidth);
		}
	}
}

void Convolution::initWeights()
{
	const bool is2d = m_kernelMode == Mode_2d || m_kernelMode == Mode_2dBilinear;

	m_weights = FRM_NEW_ARRAY(float, m_kernelSize);
	switch (m_kernelType) 
	{
		case Type_Box:
			for (int i = 0; i < m_kernelSize; ++i) 
			{
				m_weights[i] = 1.0f / (float)m_kernelSize;
			}
			m_kernelSum = 1.0f;
			break;
		case Type_Gaussian:
			if (is2d) 
			{
				m_kernelSum = KernelGaussian2d(m_kernelWidth, m_gaussianSigma, m_weights);
			} 
			else 
			{
				m_kernelSum = KernelGaussian1d(m_kernelWidth, m_gaussianSigma, m_weights);
			}
			m_gaussianSigmaOptimal = GaussianFindSigma(m_kernelWidth, 1.0f / 255.0f);
			break;
		case Type_Binomial:
			if (is2d) 
			{
				m_kernelSum = KernelBinomial2d(m_kernelWidth, m_weights);
			} 
			else 
			{
				m_kernelSum = KernelBinomial1d(m_kernelWidth, m_weights);
			}
			break;
		default:
			FRM_ASSERT(false);
			break;
	};
	m_displayWeights = FRM_NEW_ARRAY(float, m_kernelWidth);
	memcpy(m_displayWeights, m_weights, sizeof(float) * m_kernelWidth);

 // optimize for bilinear modes
	switch (m_kernelMode) 
	{
		case Mode_2dBilinear: 
		{
			m_kernelSize = m_kernelWidth / 2 + 1;
			m_kernelSize = m_kernelSize * m_kernelSize;
			float* weightsOpt = FRM_NEW_ARRAY(float, m_kernelSize);
			float* offsetsOpt = FRM_NEW_ARRAY(float, m_kernelSize * 2);
			KernelOptimizeBilinear2d(m_kernelWidth, m_weights, weightsOpt, (vec2*)offsetsOpt);
			FRM_DELETE_ARRAY(m_weights);
			FRM_DELETE_ARRAY(m_offsets);
			m_weights = weightsOpt;
			m_offsets = offsetsOpt;
			break;
		}
		case Mode_SeparableBilinear: 
		{
			m_kernelSize = m_kernelWidth / 2 + 1;
			float* weightsOpt = FRM_NEW_ARRAY(float, m_kernelSize);
			float* offsetsOpt = FRM_NEW_ARRAY(float, m_kernelSize);
			KernelOptimizeBilinear1d(m_kernelWidth, m_weights, weightsOpt, offsetsOpt);
			FRM_DELETE_ARRAY(m_weights);
			FRM_DELETE_ARRAY(m_offsets);
			m_weights = weightsOpt;
			m_offsets = offsetsOpt;
			break;
		}
		default:
			break;
	}
}

void Convolution::copyWeightsToClipboard()
{
	bool is2d = m_kernelMode == Mode_2d || m_kernelMode == Mode_2dBilinear;
	bool isBilinear = m_kernelMode == Mode_2dBilinear || m_kernelMode == Mode_SeparableBilinear;

	int cols = isBilinear ? m_kernelWidth / 2 + 1 : m_kernelWidth;
	int rows = is2d ? cols : 1;

	String<128> clipboardStr;
	for (int i = 0; i < rows; ++i) 
	{
		for (int j = 0; j < cols; ++j) 
		{
			int k = i * cols + j;
			clipboardStr.appendf("%f, ", m_weights[k]);
		}
		clipboardStr.appendf("\n");
	}
	ImGui::SetClipboardText(clipboardStr.c_str());
}

void Convolution::copyOffsetstoClipboard()
{
	bool is2d = m_kernelMode == Mode_2d || m_kernelMode == Mode_2dBilinear;
	bool isBilinear = m_kernelMode == Mode_2dBilinear || m_kernelMode == Mode_SeparableBilinear;

	int cols = isBilinear ? m_kernelWidth / 2 + 1 : m_kernelWidth;
	int rows = is2d ? cols : 1;

	String<128> clipboardStr;
	for (int i = 0; i < rows; ++i) 
	{
		for (int j = 0; j < cols; ++j) 
		{
			int k = i * cols + j;
			if (is2d) 
			{
				clipboardStr.appendf("vec2(%f, %f), ", m_offsets[k * 2], m_offsets[k * 2 + 1]);
			} 
			else 
			{
				clipboardStr.appendf("%f, ", m_offsets[k]);
			}
		}
		clipboardStr.appendf("\n");
	}
	ImGui::SetClipboardText(clipboardStr.c_str());
}
