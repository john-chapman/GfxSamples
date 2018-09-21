#include "Material.h"

#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>
#include <apt/Json.h>
#include <apt/Serializer.h>

#include <EASTL/vector.h>

using namespace frm;
using namespace apt;

// PUBLIC

Material* Material::Create(const char* _path)
{
	File f;
	if (!FileSystem::Read(f, _path)) {
		return nullptr;
	}

	Material* ret = APT_NEW(Material());
	ret->m_path = _path;
	if (FileSystem::CompareExtension("json", _path)) {
		Json json;
		Json::Read(json, f);
		SerializerJson serializer(json, SerializerJson::Mode_Read);
		APT_VERIFY(::Serialize(serializer, *ret));

	} else {
		APT_ASSERT(false); // only json implemented

	}

	return ret;
}

void Material::Release(Material* _res)
{
	APT_DELETE(_res);
}


bool Serialize(Serializer& _serializer_, Material& _res_)
{	
	APT_ASSERT(_serializer_.getMode() == Serializer::Mode_Read); // \todo implement write
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		APT_ASSERT(_res_.m_passList.empty()); // already serialized?

	 // \todo this validation could be moved into common code
		String<32> className;
		APT_VERIFY(Serialize(_serializer_, className, "_class"));
		if (className != "Material") {
			APT_LOG_ERR("Material (%s): invalid file '%s'.", FileSystem::StripPath(_res_.m_path.c_str()));
			return false;
		}
		int version;
		APT_VERIFY(Serialize(_serializer_, version, "_version"));
		if (version > Material::kVersion) {
			APT_LOG_ERR("Material (%s): incompatible version %d (min is %d).", FileSystem::StripPath(_res_.m_path.c_str()), version, Material::kVersion);
			return false;
		}

		Material::PassData defaultPassData;
		Material::Serialize(_serializer_, defaultPassData);
	
		if (_serializer_.beginObject("pass_list")) {
			while (_serializer_.beginObject()) {
				_res_.m_passList.push_back();
				auto& pass = _res_.m_passList.back();
				pass.first = _serializer_.getName();
				pass.second = defaultPassData;
				Material::Serialize(_serializer_, pass.second);

				_serializer_.endObject(); // pass
			}

			_serializer_.endObject(); // pass_list
		}

	}

	return true;
}

// PRIVATE

bool Material::Serialize(Serializer& _serializer_, PassData& _passData_)
{
	typedef String<32> Str;

	APT_ASSERT(_serializer_.getMode() == Serializer::Mode_Read);

	bool ret = true;

	if (_serializer_.beginObject("shader")) {
		PathStr path;
		eastl::vector<Str> defines;
		APT_VERIFY(_serializer_.value(path, "path"));
//path.append(".glsl"); // \todo
		uint defineCount;
		if (_serializer_.beginArray(defineCount, "defines")) {
			defines.reserve(defineCount);
			Str define;
			while (_serializer_.value(define)) {
				defines.push_back(define);
			}
			_serializer_.endArray();
		}

		ShaderDesc shDesc;
		shDesc.setPath(GL_VERTEX_SHADER,   (const char*)path);
		shDesc.setPath(GL_FRAGMENT_SHADER, (const char*)path);
		for (auto& define : defines) {
			shDesc.addGlobalDefine((const char*)define);
		}
		_passData_.m_shader = Shader::Create(shDesc);
		ret &= _passData_.m_shader != nullptr;

		_serializer_.endObject(); // shader
	}

	if (_serializer_.beginObject("states")) {
		APT_ASSERT(false);
		_serializer_.endObject(); // states
	} else {
		Str state;
		if (_serializer_.value(state, "states")) {
			_passData_.m_state = state.c_str();
		}
	}

	if (_serializer_.beginObject("texture_list")) {
		while (_serializer_.beginObject()) {
			_passData_.m_textures.push_back();
			auto& texture = _passData_.m_textures.back();
			texture.first = _serializer_.getName();
			PathStr path;
			APT_VERIFY(_serializer_.value(path));
path.append(".tga"); // \todo
			texture.second = Texture::Create(path.c_str());
			ret &= texture.second != nullptr;
			if (texture.second) {
				texture.second->setName(texture.first.c_str());
			}

			_serializer_.endObject(); // texture
		}
		_serializer_.endObject(); // texture_list
	}

	return true;
}

Material::Material()
{
}

Material::~Material()
{
}
