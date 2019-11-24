#include "BlockCompression.h"

#include <frm/core/frm.h>
#include <frm/core/gl.h>
#include <frm/core/log.h>
#include <frm/core/ArgList.h>
#include <frm/core/Buffer.h>
#include <frm/core/FileSystem.h>
#include <frm/core/GlContext.h>
#include <frm/core/Image.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>
#include <frm/core/Window.h>

using namespace frm;

static BlockCompression s_inst;

BlockCompression::BlockCompression()
	: AppBase("BlockCompression") 
{
	PropertyGroup& propGroup = m_props.addGroup("BlockCompression");
	//                name                      default                 min     max          storage
	propGroup.addInt ("m_mode",                 m_mode,                 0,      Mode_Count,  &m_mode);
	propGroup.addBool("m_tests[0].m_usePCA",    m_tests[0].m_usePCA,                         &m_tests[0].m_usePCA);
	propGroup.addBool("m_tests[1].m_usePCA",    m_tests[1].m_usePCA,                         &m_tests[1].m_usePCA);
	propGroup.addPath("m_txSrcPath",            "kodim23.png",                               &m_txSrcPath);
}

BlockCompression::~BlockCompression()
{
}

bool BlockCompression::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args)) 
	{
		return false;
	}

	m_shView = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/View_fs.glsl");

	if (!initSourceTexture())
	{
		return false;
	}

	for (auto& test : m_tests)
	{
		if (!initShaders(test))
		{
			return false;
		}
		if (!initTextures(test))
		{
			return false;
		}
	}

	return true;
}

void BlockCompression::shutdown()
{
	for (auto& test : m_tests)
	{
		shutdownTextures(test);
		shutdownShaders(test);
	}
	shutdownSourceTexture();
	Shader::Release(m_shView);

	AppBase::shutdown();
}

bool BlockCompression::update()
{
	if (!AppBase::update()) 
	{
		return false;
	}

	ImGui::Combo("Mode", &m_mode,
		"None\0"
		"Source\0"
		"Error\0"
		"Block EP0\0"
		"Block EP1\0"
		);
	
	if (ImGui::Button("Source Image"))
	{
		if (FileSystem::PlatformSelect(m_txSrcPath)) 
		{
			m_txSrcPath = FileSystem::MakeRelative(m_txSrcPath.c_str());
			initSourceTexture();
			for (auto& test : m_tests)
			{
				initTextures(test);
			}
		}
	}
	ImGui::SameLine();
	ImGui::Text(m_txSrcPath.c_str());

	{
		uint8 block[(4 * 4) * 4];
		getSourceBlock(m_imgSrc, m_previewRectXY.x, m_previewRectXY.y, block);
	}

	for (auto& test : m_tests)
	{
		if (test.m_shBc1->getState() == Shader::State_Loaded)
		{	PROFILER_MARKER("BC1 Compression");

			auto ctx = GlContext::GetCurrent();

			{	PROFILER_MARKER("Compress");
				ctx->setShader(test.m_shBc1);
				ctx->bindTexture("txSrc", m_txSrc);
				ctx->bindBuffer("_bfDst", test.m_bfDst);
				ctx->dispatch(m_txSrc);
			}
					
			{	PROFILER_MARKER("Copy");
				glAssert(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT));
				glScopedPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glAssert(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, test.m_bfDst->getHandle()));
				glAssert(glCompressedTextureSubImage2D(test.m_txDst->getHandle(), 0, 0, 0, test.m_txDst->getWidth(), test.m_txDst->getHeight(), test.m_txDst->getFormat(), test.m_bfDst->getSize(), 0));
				glAssert(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
				glAssert(glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT));
			}
		}
	}

	return true;
}

void BlockCompression::draw()
{
	GlContext* ctx = GlContext::GetCurrent();

	int border     = 20;
	int w          = m_windowSize.x / 2 - border * 2;
	int h          = (int)(((float)m_txSrc->getHeight() / (float)m_txSrc->getWidth()) * w);
	    h          = Min(h, m_windowSize.y * 2/3);
		w          = (int)(((float)m_txSrc->getWidth() / (float)m_txSrc->getHeight()) * h);
	int xl         = border;
	int xr         = border * 2 + w;
	int y          = border;

	drawPreview(xl, y, w, h, m_tests[0]);
	drawPreview(xr, y, w, h, m_tests[1]);

	AppBase::draw();
}

bool BlockCompression::initSourceTexture()
{
	shutdownSourceTexture();
	
	File f;
	if (!FileSystem::Read(f, m_txSrcPath.c_str()))
	{
		return false;
	}

	m_imgSrc = FRM_NEW(Image);
	if (!Image::Read(*m_imgSrc, f))
	{
		return false;
	}

	m_txSrc = Texture::Create(*m_imgSrc);
	m_txSrc->setName("txSrc");
	m_txSrc->setMagFilter(GL_NEAREST);

	return true;
}

void BlockCompression::shutdownSourceTexture()
{
	Texture::Release(m_txSrc);
	FRM_DELETE(m_imgSrc);
}

bool BlockCompression::initTextures(Test& _test_)
{
	shutdownTextures(_test_);

	if (!m_txSrc)
	{
		FRM_LOG_ERR("initTextures: m_txSrc was not loaded");
		return false;
	}

	uint32 bfSize = (m_txSrc->getWidth()/4 * m_txSrc->getHeight()/4) * 8; // 4x4 blocks, 8 bytes per block
	_test_.m_bfDst = Buffer::Create(GL_SHADER_STORAGE_BUFFER, bfSize, GL_DYNAMIC_STORAGE_BIT);
	if (!_test_.m_bfDst)
	{
		return false;
	}
	_test_.m_bfDst->setName("_bfDst");

	_test_.m_txDst = Texture::Create2d(m_txSrc->getWidth(), m_txSrc->getHeight(), GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
	if (!_test_.m_txDst)
	{
		return false;
	}
	_test_.m_txDst->setName("txDst");
	_test_.m_txDst->setFilter(GL_NEAREST);
	_test_.m_txDst->setWrap(GL_CLAMP_TO_BORDER);
	
	return true;
}

void BlockCompression::shutdownTextures(Test& test_)
{
	Buffer::Destroy(test_.m_bfDst);
	Texture::Release(test_.m_txDst);
}

bool BlockCompression::initShaders(Test& _test_)
{
	shutdownShaders(_test_);

	ShaderDesc shDesc;
	shDesc.setPath(GL_COMPUTE_SHADER, "shaders/BC1_cs.glsl");
	shDesc.setLocalSize(4, 4, 1);
	shDesc.addGlobalDefine("USE_PCA", _test_.m_usePCA ? 1 : 0);
	_test_.m_shBc1 = Shader::Create(shDesc);

	return _test_.m_shBc1->getState() != Shader::State_Error;
}

void BlockCompression::shutdownShaders(Test& test_)
{
	Shader::Release(test_.m_shBc1);
}

void BlockCompression::drawPreview(int _x, int _y, int _w, int _h, Test& _test)
{
	const vec2 mainXY = vec2(_x, _y);
	const vec2 mainWH = vec2(_w, _h);
	const vec2 subXY  = vec2(_x, _y + _h + 2);
	const vec2 subWH  = vec2(200.0f);
	
	const float subSize = 20.0f;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
	ImGui::SetNextWindowPos(vec2(0, 0));
	ImGui::SetNextWindowSize(vec2(m_windowSize));
	ImGui::Begin("main", nullptr, 0
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		);

	ImGui::PushID(&_test);
	auto drawList = ImGui::GetWindowDrawList();
	drawList->AddRect(mainXY, mainXY + mainWH, ImGui::GetColorU32(ImGuiCol_Border));
	drawList->AddRect(subXY,  subXY  + subWH,  ImGui::GetColorU32(ImGuiCol_Border));
	static vec2 hoverUv = vec2(0.5f);
	ImGui::SetCursorPos(mainXY);
	ImGui::InvisibleButton("###preview", mainWH);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDown(1)) 
	{
		hoverUv = (vec2(ImGui::GetMousePos()) - vec2(ImGui::GetItemRectMin())) / mainWH;
	}
	drawList->AddRect(Floor(mainXY + hoverUv * mainWH - vec2(subSize * 0.5f)), Floor(mainXY + hoverUv * mainWH + vec2(subSize * 0.5f)), IM_COL32_WHITE);

	ImGui::SetCursorPos(ImVec2(subXY.x + subWH.x + 4.0f, subXY.y));
	bool reinit = false;
	reinit |= ImGui::Checkbox("Use PCA", &_test.m_usePCA);
	if (reinit)
	{
		initShaders(_test);
	}

	auto ctx = GlContext::GetCurrent();
	ctx->setFramebuffer(0);
	ctx->setShader(m_shView);
	ctx->setViewport(_x, m_windowSize.y - _y - _h, _w, _h);
	ctx->setUniform("uMode", m_mode);
	ctx->setUniform("uUvScale", vec2(1.0f));
	ctx->setUniform("uUvBias", vec2(0.0f));
	ctx->bindTexture("txSrc", _test.m_txDst);
	ctx->bindTexture("txCmp", m_txSrc);
	ctx->bindBuffer("_bfSrc", _test.m_bfDst);
	ctx->drawNdcQuad();

	vec2 uvScale = vec2(subSize) / vec2(m_txSrc->getWidth(), m_txSrc->getHeight());
	ctx->setViewport(_x, m_windowSize.y - (int)subXY.y - (int)subWH.y, (int)subWH.x, (int)subWH.y);
	ctx->setUniform("uUvBias", hoverUv - uvScale * 0.5f);
	ctx->setUniform("uUvScale", uvScale);
	ctx->drawNdcQuad();

	ImGui::PopID();
	ImGui::End();
	ImGui::PopStyleColor(1);

	m_previewRectXY = ivec2(hoverUv * vec2(m_txSrc->getWidth(), m_txSrc->getHeight()));
	drawList->AddRect(Floor(mainXY + hoverUv * mainWH - vec2(subSize * 0.5f)), Floor(mainXY + hoverUv * mainWH + vec2(subSize * 0.5f)), IM_COL32_WHITE);

}

void BlockCompression::getSourceBlock(Image* _img, int _x, int _y, uint8 block_[(4* 4) * 4])
{
	FRM_ASSERT(_img->getImageDataType() == DataType_Uint8N);

	_x = FRM_CLAMP(_x, 0, (int)_img->getWidth());
	_y = FRM_CLAMP(_y, 0, (int)_img->getHeight());

	uint    texelSizeBytes = (uint)_img->getBytesPerTexel();
	uint    stride         = _img->getWidth() * texelSizeBytes;
	ivec2   blockXY        = ivec2(_x, _y) / ivec2(4) * ivec2(4);
	uint8N* src            = (uint8N*)_img->getRawImage() + blockXY.y * stride + blockXY.x * texelSizeBytes;

	for (int y = 0; y < 4; ++y)
	{
		uint8N* row = src + y * stride;
		for (int x = 0; x < 4; ++x)
		{
			for (int z = 0; z < 3; ++z)
			{
				*block_ = *row;
				++block_;
				++row;
			}
			if (_img->getLayout() == Image::Layout_RGBA)
			{
				*block_ = *row;
			} 
			else
			{
				*block_ = FRM_DATA_TYPE_MAX(uint8N);
			}
			++block_;
		}
	}
}

