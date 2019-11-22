// Welcome to the GfxSampleFramework tutorial! 
//
// The GfxSampleFramework uses the namespace 'frm'.
//
// The basis of any program written with the framework is the application class, which derives from a base class (AppSample3d in this case). The base
// class does some common stuff like processing input and drawing the UI and debug lines. Your application needs to provide 4 functions:
//
//   init()      - Called once when the application starts up, intialize and load resources.
//   shutdown()  - Called once when the application closes, release resources.
//   update()    - Called every frame before draw(), update application state.
//   draw()      - Called every frame, draw stuff!
//
// The base app provides a number of features, accessible via the following shortcuts:
//
//   F1          - Toggle the main menu bar + status bar. Tip: you can click the status message in the lower left to see the log window.
//   F2          - Toggle the 3d wireframe 'helpers'.
//   F8          - Force reload all textures (auto reload is enabled for textures in the data/ dir).
//   F9          - Force reload all shaders (auto reload is enabled for shaders in the data/ dir).
//   Ctrl+0      - Toggle the scene editor.
//   Ctrl+1      - Toggle the profiler.
//   Ctrl+2      - Toggle the texture viewer.
//   Ctrl+3      - Toggle the shader viewer.
//   Shift+Esc   - Quit.
//
// Scene Editor (Ctrl+0)
// =====================
// The scene editor is WIP - it's mostly useful if you want to set up multiple cameras. By default the scene has a single free camera with WASD 
// control, left shift to increase movement speed, right click + drag to look around. The scene separates the notion of the 'draw' and 'cull' 
// cameras, this can be useful for testing. For example by pressing Ctrl+LShift+C you can 'freeze' the cull camera in place and then continue to
// move around the scene with the draw camera until you press Ctrl+LShift+C again.
//
// Profiler (Ctrl+1)
// =================
// By default, the 'marker' view is shown with GPU markers on top (blue) and CPU markers below (yellow). The pause/break key pauses the 
// profiler. Mouse wheel zooms in/out, double clicking on a marker zooms in such that the marker fills the window. When paused, right-click a 
// marker to 'track' it - this pins a graph view to the main window and tracks the average, min and max duration over time.
//
// See the code below for examples of declaring profiler markers.
//
// Texture Viewer (Ctrl+2)
// =======================
// The texture viewer shows all texture resources loaded by the application. Click a thumbnail to go to the detail view, from which you can modify
// the default sampler state for the texture as well as save the texture to disk.
//
// Shader Viewer (Ctrl+3)
// ======================
// The shader viewer shows dependencies, uniform names and defines for a compiled shader program, as well as the loaded source code.
//
#include "Tutorial.h"

#include <frm/core/frm.h>
#include <frm/core/ArgList.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Profiler.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

using namespace frm;

static Tutorial s_inst;

Tutorial::Tutorial()
	: AppBase("Tutorial") 
{
}

Tutorial::~Tutorial()
{
}

bool Tutorial::init(const ArgList& _args)
{
	if (!AppBase::init(_args)) {
		return false;
	}

 // Loading a texture from disk is trvial - most common image data types are supported.
	m_txDiffuse = Texture::Create("textures/baboon.png");
	FRM_ASSERT(m_txDiffuse); // Create() returns a nullptr if the texture didn't load!
	m_txDiffuse->generateMipmap(); // if the disk texture didn't have a mipmap we can generate one

 // Creating a texture via Create*() is used for render targets.
	m_txScene = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
	m_txScene->setName("txScene"); // name is used in the texture viewer, or for binding if no name is explicitly specified (see draw()).
	m_txScene->setWrap(GL_CLAMP_TO_EDGE); // can modify sampler mode, etc. after creating the texture
	m_txSceneDepth = Texture::Create2d(m_resolution.x, m_resolution.y, GL_DEPTH32F_STENCIL8);
	m_txSceneDepth->setName("txSceneDepth");

 // Creating a framebuffer is simply a matter of passing the number of bindings plus the texture pointers (color targets are bound in the order specified).
	m_fbScene = Framebuffer::Create(2, m_txScene, m_txSceneDepth);

 // Loading a mesh from disk is also trivial, but only .obj is currently supported :'(
	m_mesh = Mesh::Create("models/teapot.obj");

 // Vertex/fragment shaders can be loaded and compiled in a single step, with optional defines.
	m_shMesh = Shader::CreateVsFs("shaders/Mesh_vs.glsl", "shaders/Mesh_fs.glsl", { "DEFINE_ONE 1", "DEFINE_TWO 2", "DEFINE_THREE 3" });
	FRM_ASSERT(m_shMesh); // Create*() returns a nullptr if loading, compiling or linking the shader failed (check the output log in this case).

 // Compute shaders are loaded in the same way, but we must specify the group size.
	m_shPostProcess = Shader::CreateCs("shaders/PostProcess_cs.glsl", 16, 16, 1);
	FRM_ASSERT(m_shPostProcess);

	return true;
}

void Tutorial::shutdown()
{
 // Shaders, textures and meshes are shared resources, so we call 'Release' rather 'Destroy'.
	Shader::Release(m_shMesh);
	Shader::Release(m_shPostProcess);
	Texture::Release(m_txDiffuse);
	Texture::Release(m_txSceneDepth);
	Texture::Release(m_txScene);
	Mesh::Release(m_mesh);

 // Framebuffers aren't a shared resource, call 'Destroy' directly.
	Framebuffer::Destroy(m_fbScene); // txScene and txSceneDepth will actually only get destroyed here

	
	AppBase::shutdown();
}

bool Tutorial::update()
{
	if (!AppBase::update()) {
		return false;
	}
	
 // Im3d (https://github.com/john-chapman/im3d) provides a simple immediate-mode gizmos for manipulating transforms, use Ctrl+T, Ctrl+R, Ctrl+S to switch between translation/rotation/scale.
	Im3d::Gizmo("world matrix", (float*)&m_worldMatrix);

 // Im3d also has an immediate mode drawing API which is very useful for testing and debugging 3d stuff.
	Im3d::PushColor(Im3d::Color_Magenta);
	Im3d::PushMatrix(m_worldMatrix * ScaleMatrix(vec3(m_scale)));
		Im3d::DrawAlignedBox(m_mesh->getBoundingBox().m_min, m_mesh->getBoundingBox().m_max);
	Im3d::PopMatrix();
	Im3d::PopColor();


 // ImGui (https://github.com/ocornut/imgui) is integrated to provide an immediate-mode UI for building simple tools and debugging.
	ImGui::Begin("Tutorial");
		ImGui::SliderFloat("Scale", &m_scale, 0.0f, 10.0f);
	ImGui::End();

	return true;
}

void Tutorial::draw()
{
 // 'Draw' and 'cull' cameras are separated. Press Ctrl+LShift+C to 'freeze' the cull camera in place while moving the draw camera until the shortcut is pressed again.
	auto drawCam = Scene::GetDrawCamera();
	auto cullCam = Scene::GetCullCamera();

 // All graphics operations happen on the current GlContext.
	auto ctx = GlContext::GetCurrent();

 // Example: drawing objects in 3d with the draw/cull cameras
	{	PROFILER_MARKER("Scene"); // scoped marker for CPU + GPU, use PROFILER_MARKER_CPU/PROFILER_MARKER_GPU 

		ctx->setFramebufferAndViewport(m_fbScene);
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // can make gl* calls directly, wrap with glAssert() to check the error state
		glScopedEnable(GL_DEPTH_TEST, GL_TRUE); // glScopedEnable() is convenient for setting gl state within a scope block only 
	
		ctx->setShader(m_shMesh);
		ctx->bindTexture("txDiffuse", m_txDiffuse);
		ctx->bindBuffer(drawCam->m_gpuBuffer);
	
		auto worldMatrix = m_worldMatrix * ScaleMatrix(vec3(m_scale));
		auto boundingBox = m_mesh->getBoundingBox();
		boundingBox.transform(worldMatrix);
		if (cullCam->m_worldFrustum.inside(boundingBox)) {
			ctx->setUniform("uWorldMatrix", worldMatrix);
			ctx->setMesh(m_mesh);
			ctx->draw();
		}
	}

 // Example: compute shader with a texture bound as an image variable.
	{	PROFILER_MARKER("PostProcess");

		ctx->setShader(m_shPostProcess);
		ctx->bindImage("txScene", m_txScene, GL_READ_WRITE);
		ctx->dispatch(m_txScene); // dispatch enough groups to creat at least 1 thread per texel
	}

	ctx->blitFramebuffer(m_fbScene, nullptr);

	AppBase::draw();
}
