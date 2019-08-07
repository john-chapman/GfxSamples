#include "_skeleton.h"

#include <frm/core/def.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <apt/ArgList.h>

using namespace frm;
using namespace apt;

static _skeleton s_inst;

_skeleton::_skeleton()
	: AppBase("_skeleton") 
{
}

_skeleton::~_skeleton()
{
}

bool _skeleton::init(const apt::ArgList& _args)
{
	if (!AppBase::init(_args)) {
		return false;
	}

 // code here

	return true;
}

void _skeleton::shutdown()
{
 // code here

	AppBase::shutdown();
}

bool _skeleton::update()
{
	if (!AppBase::update()) {
		return false;
	}

 // code here

	return true;
}

void _skeleton::draw()
{
 // code here

	AppBase::draw();
}
