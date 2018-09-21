#pragma once

#include <frm/core/def.h>

#include <apt/String.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/vector_map.h>

///////////////////////////////////////////////////////////////////////////////
// Material
// 
///////////////////////////////////////////////////////////////////////////////
class Material
{
public:
	static constexpr int kVersion = 0;

	struct PassData
	{
		frm::Shader* m_shader = nullptr;
		apt::String<16> m_state;//frm::uint32  m_stateId = 0; // \todo
		eastl::vector_map<apt::String<16>, frm::Texture*> m_textures;
	};

	static Material* Create(const char* _path);
	static void Release(Material* _mat);

	int findPass(const char* _name) const;
    const PassData& getPass(int _pass) const { return m_passList[_pass].second; }
	

	friend bool Serialize(apt::Serializer& _serializer_, Material& _res_);

private:
	apt::PathStr m_path;
	eastl::fixed_vector<eastl::pair<apt::String<8>, PassData>, 4> m_passList;

	static bool Serialize(apt::Serializer& _serializer_, PassData& _passData_);
	
	Material();
	~Material();
}; 

bool Serialize(apt::Serializer& _serializer_, Material& _res_);

inline int Material::findPass(const char* _name) const
{
 // first LodList is the 'default', start the search at 1
	for (int i = 1, n = (int)m_passList.size(); i < n; ++i) {
		if (m_passList[i].first == _name) { // \todo could me more complex e.g. a comma separated list
			return i;
		}
	} 

	return 0;
}