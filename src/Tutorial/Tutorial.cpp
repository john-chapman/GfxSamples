#include "Tutorial.h"

#include <frm/core/def.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <apt/ArgList.h>

using namespace frm;
using namespace apt;

static Tutorial s_inst;

Tutorial::Tutorial()
	: AppBase("Tutorial") 
{
}

Tutorial::~Tutorial()
{
}

bool Tutorial::init(const apt::ArgList& _args)
{
	if (!AppBase::init(_args)) {
		return false;
	}

 // code here

	return true;
}

void Tutorial::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool Tutorial::update()
{
	if (!AppBase::update()) {
		return false;
	}

 // code here

	return true;
}

void Tutorial::draw()
{
 // code here

	AppBase::draw();
}
