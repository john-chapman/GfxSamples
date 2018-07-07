#include "BlockCompression.h"

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/Buffer.h>
#include <frm/GlContext.h>
#include <frm/Profiler.h>
#include <frm/Shader.h>
#include <frm/Texture.h>
#include <frm/Window.h>

#include <apt/ArgList.h>

using namespace frm;
using namespace apt;

static BlockCompression s_inst;

BlockCompression::BlockCompression()
	: AppBase("BlockCompression") 
{
	PropertyGroup& propGroup = m_props.addGroup("BlockCompression");
	//                  name             default            min     max    storage
	//propGroup.addFloat  ("Float",        0.0f,              0.0f,   1.0f,  &foo);
	propGroup.addPath("Source Image Path", "kodim23.png", &m_txSrcPath);
}

BlockCompression::~BlockCompression()
{
}

bool BlockCompression::init(const apt::ArgList& _args)
{
	if (!AppBase::init(_args)) {
		return false;
	}

	initTextures();

	m_shBc1  = Shader::CreateCs("shaders/Bc1_cs.glsl", 4, 4);
	m_shView = Shader::CreateVsFs("shaders/Basic_vs.glsl", "shaders/View_fs.glsl");

	return true;
}

void BlockCompression::shutdown()
{
	shutdownTextures();

	Shader::Release(m_shView);

	AppBase::shutdown();
}

bool BlockCompression::update()
{
	if (!AppBase::update()) {
		return false;
	}

	ImGui::Combo("Mode", &m_mode,
		"None\0"
		"Difference\0"
		);

	if (m_shBc1->getState() == Shader::State_Loaded)
	{	PROFILER_MARKER("BC1 Compression");

		auto ctx = GlContext::GetCurrent();

		{	PROFILER_MARKER("Compress");
			ctx->setShader(m_shBc1);
			ctx->bindTexture("txSrc", m_txSrc);
			ctx->bindBuffer("_bfDst", m_bfDst);
			auto dispatchCount = m_shBc1->getDispatchSize(m_txSrc->getWidth() / 4, m_txSrc->getHeight() / 4);
			ctx->dispatch(m_txSrc);
		}
				
		{	PROFILER_MARKER("Copy");
			glAssert(glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT));
			FRM_GL_PIXELSTOREI(GL_UNPACK_ALIGNMENT, 1);
			glAssert(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_bfDst->getHandle()));
			glAssert(glCompressedTextureSubImage2D(m_txDst->getHandle(), 0, 0,0, m_txDst->getWidth(), m_txDst->getHeight(), m_txDst->getFormat(), m_bfDst->getSize(), 0));
			glAssert(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
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
	int xl         = border;
	int xr         = border * 2 + w;
	int y          = border;

	drawPreview(xl, y, w, h, m_txCmp, m_txSrc);
	drawPreview(xr, y, w, h, m_txDst, m_txSrc);

	AppBase::draw();
}

bool BlockCompression::initTextures()
{
	shutdownTextures();
	
	m_txSrc = Texture::Create((const char*)m_txSrcPath);
	m_txSrc->setName("txSrc");
	m_txSrc->setFilter(GL_NEAREST);
	m_txSrc->setWrap(GL_CLAMP_TO_BORDER);

	uint32 bfSize = (m_txSrc->getWidth()/4 * m_txSrc->getHeight()/4) * 8; // 4x4 blocks, 8 bytes per block
	m_bfDst = Buffer::Create(GL_SHADER_STORAGE_BUFFER, bfSize, GL_DYNAMIC_STORAGE_BIT);
	m_bfDst->setName("_bfDst");

	m_txDst = Texture::Create2d(m_txSrc->getWidth(), m_txSrc->getHeight(), GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
	m_txDst->setName("txDst");
	m_txDst->setFilter(GL_NEAREST);
	m_txDst->setWrap(GL_CLAMP_TO_BORDER);
	
	PathStr cmpPath("%s/%s.dds",
		(const char*)FileSystem::GetPath((const char*)m_txSrcPath),
		(const char*)FileSystem::GetFileName((const char*)m_txSrcPath)
		);
	m_txCmp = Texture::Create((const char*)cmpPath);
	if (m_txCmp->getState() == Texture::State_Loaded) {
		m_txCmp->setName("txCmp");
		m_txCmp->setFilter(GL_NEAREST);
		m_txCmp->setWrap(GL_CLAMP_TO_BORDER);
	}

	return true;
}

void BlockCompression::shutdownTextures()
{
	Texture::Release(m_txSrc);
	Texture::Release(m_txCmp);
	Buffer::Destroy(m_bfDst);
	Texture::Release(m_txDst);
}

void BlockCompression::drawPreview(int _x, int _y, int _w, int _h, frm::Texture* _txSrc, frm::Texture* _txCmp)
{
	const vec2 mainXY = vec2(_x, _y);
	const vec2 mainWH = vec2(_w, _h);
	const vec2 subXY  = vec2(_x, _y + _h + 2.0f);
	const vec2 subWH  = vec2(200.0f);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
	ImGui::SetNextWindowPos(vec2(0, 0));
	ImGui::SetNextWindowSize(vec2(m_windowSize));
	ImGui::Begin("fullscreen", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus
		);

	auto drawList = ImGui::GetWindowDrawList();
	drawList->AddRect(mainXY, mainXY + mainWH, ImGui::GetColorU32(ImGuiCol_Border));
	drawList->AddRect(subXY,  subXY  + subWH,  ImGui::GetColorU32(ImGuiCol_Border));
	static vec2 hoverUv = vec2(0.5f);
	ImGui::SetCursorPos(mainXY);
	ImGui::InvisibleButton("###preview", mainWH);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDown(1)) {
		hoverUv = (vec2(ImGui::GetMousePos()) - vec2(ImGui::GetItemRectMin())) / mainWH;
	}
	drawList->AddRect(Floor(mainXY + hoverUv * mainWH - vec2(10.0f)), Floor(mainXY + hoverUv * mainWH + vec2(10.0f)), IM_COL32_WHITE);

	auto ctx = GlContext::GetCurrent();
	ctx->setFramebuffer(0);
	ctx->setShader(m_shView);
	ctx->setViewport(_x, m_windowSize.y - _y - _h, _w, _h);
	ctx->setUniform("uMode", m_mode);
	ctx->setUniform("uUvScale", vec2(1.0f));
	ctx->setUniform("uUvBias", vec2(0.0f));
	ctx->bindTexture("txSrc", _txSrc);
	ctx->bindTexture("txCmp", _txCmp);
	ctx->drawNdcQuad();

	vec2 uvScale = vec2(20.0f) / vec2(m_txSrc->getWidth(), m_txSrc->getHeight());
	ctx->setViewport(_x, m_windowSize.y - (int)subXY.y - (int)subWH.y, (int)subWH.x, (int)subWH.y);
	ctx->setUniform("uUvBias", hoverUv - uvScale * 0.5f);
	ctx->setUniform("uUvScale", uvScale);
	ctx->drawNdcQuad();

	ImGui::End();
	ImGui::PopStyleColor(1);
}

