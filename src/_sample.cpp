#include <frm/core/frm.h>
#include <frm/core/gl.h>
#include <frm/core/ArgList.h>
#include <frm/core/AppSample.h>
#include <frm/core/FileSystem.h>
#include <frm/core/GlContext.h>
#include <frm/core/Window.h>

using namespace frm;

int main(int _argc, char** _argv)
{
	FileSystem::AddRoot("sample_common");
	AppSample* app = AppSample::GetCurrent();
	if (!app->init(ArgList(_argc, _argv))) 
	{
		FRM_ASSERT(false);
		return 1;
	}
	Window* win = app->getWindow();
	GlContext* ctx = app->getGlContext();
	while (app->update()) 
	{
		FRM_VERIFY(GlContext::MakeCurrent(ctx));
		ctx->setFramebufferAndViewport(0);
		glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
		app->draw();
	}
	app->shutdown();
	return 0;
}
