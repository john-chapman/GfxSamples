#include "_skeleton.h"

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

static _skeleton s_inst;

_skeleton::_skeleton()
	: AppBase("_skeleton") 
{
}

_skeleton::~_skeleton()
{
}

bool _skeleton::init(const frm::ArgList& _args)
{
	if (!AppBase::init(_args))
	{
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
	if (!AppBase::update())
	{
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
