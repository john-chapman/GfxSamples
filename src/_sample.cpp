#include <frm/core/def.h>
#include <frm/core/gl.h>
#include <frm/core/AppSample.h>
#include <frm/core/GlContext.h>
#include <frm/core/Window.h>

#include <apt/ArgList.h>
#include <apt/FileSystem.h>

using namespace frm;
using namespace apt;

int main(int _argc, char** _argv)
{
	FileSystem::AddRoot("sample_common");
	AppSample* app = AppSample::GetCurrent();
	if (!app->init(ArgList(_argc, _argv))) {
		//APT_ASSERT(false);
		return 1;
	}
	Window* win = app->getWindow();
	GlContext* ctx = app->getGlContext();
	while (app->update()) {
		APT_VERIFY(GlContext::MakeCurrent(ctx));
		ctx->setFramebufferAndViewport(0);
		glAssert(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
		app->draw();
	}
	app->shutdown();
	return 0;
}
