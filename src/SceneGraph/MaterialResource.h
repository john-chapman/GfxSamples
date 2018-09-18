#pragma once

#include <frm/core/def.h>

#include <apt/String.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/vector_map.h>

///////////////////////////////////////////////////////////////////////////////
// MaterialResource
// 
///////////////////////////////////////////////////////////////////////////////
class MaterialResource
{
public:
	static constexpr int kVersion = 0;

	struct PassData
	{
		frm::Shader* m_shader = nullptr;
		frm::uint32  m_stateId = 0;
		eastl::vector_map<apt::String<16>, frm::Texture*> m_textures;
	};

	static MaterialResource* Create(const char* _path);
	static void Release(MaterialResource* _mat);

	friend bool Serialize(apt::Serializer& _serializer_, MaterialResource& _res_);

private:
	apt::PathStr m_path;

	apt::PathStr m_path;
	eastl::fixed_vector<eastl::pair<apt::String<8>, PassData>, 4> m_passList;

	MaterialResource();
	~MaterialResource();

	static bool Serialize(apt::Serializer& _serializer_, PassData& _passData_);
}; 

bool Serialize(apt::Serializer& _serializer_, MaterialResource& _res_);