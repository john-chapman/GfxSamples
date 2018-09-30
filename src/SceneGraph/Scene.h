#pragma once

#include "Serializable.h"

#include <frm/core/def.h>
#include <frm/core/math.h>

#include <EASTL/vector.h>

class Model;
class Switch;

class SceneX: public Serializable<SceneX> // \todo calling it SceneX to avoid clashing with frm::Scene
{
public:
	enum NodeType
	{
		NodeType_Model,
		NodeType_Transform,
		NodeType_Scene,
		NodeType_Switch,

		NodeType_Count
	};

	static SceneX* Create(const char* _path);
	static void Release(SceneX* _res);

	friend bool Serialize(apt::Serializer& _serializer_, SceneX& _res_);



private:

	union NodeData
	{
		//frm::mat4  transform; // \todo default ctor for mat4 so can't put it directly in a union
		Model*     m_model;
		SceneX*    m_scene;
		Switch*    m_switch;
	};

	apt::PathStr m_path;

	eastl::vector<NodeType> m_nodeType;
	eastl::vector<NodeData> m_nodeData;
	eastl::vector<int>      m_nodeParent;

	
	static bool SerializeNode(apt::Serializer& _serializer_, SceneX& _scene_, int& _parent_);

	SceneX();
	~SceneX();

};

bool Serialize(apt::Serializer& _serializer_, SceneX& _res_);