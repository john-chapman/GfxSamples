#include "Scene.h"

#include "Model.h"
#include "Serializable.h"

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>
#include <apt/Json.h>
#include <apt/Serializer.h>

using namespace frm;
using namespace apt;

// \todo \hack fix namespace collision to use the macro (class is SceneX but we want to call it Scene)
//SERIALIZABLE_DEFINE(SceneX, 0);
	const char* Serializable<SceneX>::kClassName = "Scene";
	const apt::StringHash Serializable<SceneX>::kClassNameHash = apt::StringHash("Scene");
	const int Serializable<SceneX>::kClassVersion = 0;

// PUBLIC

SceneX* SceneX::Create(const char* _path)
{
	File f;
	if (!FileSystem::Read(f, _path)) 
	{
		return nullptr;
	}

	SceneX* ret = APT_NEW(SceneX());
	ret->m_path = _path;
	if (FileSystem::CompareExtension("json", _path)) 
	{
		Json json;
		Json::Read(json, f);
		SerializerJson serializer(json, SerializerJson::Mode_Read);
		APT_VERIFY(::Serialize(serializer, *ret));

	} else 
	{
		APT_ASSERT(false); // only json implemented

	}

	return ret;
}

void SceneX::Release(SceneX* _res)
{
	APT_DELETE(_res);
}

bool Serialize(Serializer& _serializer_, SceneX& _res_)
{
 // \todo better error handling in this function, can skip invalid meshes or materials without assert, only fully empty resources are illegal

	APT_ASSERT(_serializer_.getMode() == Serializer::Mode_Read); // \todo implement write
	if (_serializer_.getMode() == Serializer::Mode_Read) 
	{
		APT_ASSERT(_res_.m_nodeData.empty()); // already serialized?

	 // \todo this validation could be moved into common code
		if (!SceneX::SerializeAndValidateClassName(_serializer_)) 
		{
			return false;
		}
		if (!SceneX::SerializeAndValidateClassVersion(_serializer_)) 
		{
			return false;
		}
	}

	return true;
}

// PRIVATE

bool SceneX::SerializeNode(apt::Serializer& _serializer_, SceneX& _scene_, int& _parent_)
{
	int nodeIndex = (int)_scene_.m_nodeType.size() - 1;
	_scene_.m_nodeParent.push_back(_parent_);
	_scene_.m_nodeType.push_back();
	auto& nodeType = _scene_.m_nodeType.back();
	_scene_.m_nodeData.push_back();
	auto& nodeData = _scene_.m_nodeData.back();
	
	PathStr modelPath;
	if (_serializer_.value(modelPath, "model")) 
	{
		nodeType = NodeType_Model;
		nodeData.m_model = Model::Create(modelPath.c_str());
		return true;
	}

	mat4 transform;
	if (_serializer_.value(transform, "transform")) 
	{
		APT_ASSERT(false);
		nodeType = NodeType_Transform;
		//nodeData.m_transform.m_matrix = transform;
		return true;
	}

	PathStr scenePath;
	if (_serializer_.value(scenePath, "scene")) {
		nodeType = NodeType_Scene;
		nodeData.m_scene = SceneX::Create(scenePath.c_str());
		return true;
	}

	return false;
}

SceneX::SceneX()
{
}

SceneX::~SceneX()
{
}
