#include "SceneGraph.h"

#include "SceneGraph/MeshResource.h"

#include <frm/core/def.h>
#include <frm/core/gl.h>

#include <apt/ArgList.h>

using namespace frm;
using namespace apt;

static SceneGraph s_inst;

SceneGraph::SceneGraph()
	: AppBase("SceneGraph") 
{
	PropertyGroup& propGroup = m_props.addGroup("SceneGraph");
	//                  name             default            min     max    storage
	//propGroup.addFloat  ("Float",        0.0f,              0.0f,   1.0f,  &foo);
}

SceneGraph::~SceneGraph()
{
}

bool SceneGraph::init(const apt::ArgList& _args)
{
	if (!AppBase::init(_args)) {
		return false;
	}


	MeshResource* test = MeshResource::Create("MeshResource/teapot.json");	

	return true;
}

void SceneGraph::shutdown()
{
	// sample code here

	AppBase::shutdown();
}

bool SceneGraph::update()
{
	if (!AppBase::update()) {
		return false;
	}

	// sample code here

	return true;
}

void SceneGraph::draw()
{
	// sample code here

	AppBase::draw();
}
