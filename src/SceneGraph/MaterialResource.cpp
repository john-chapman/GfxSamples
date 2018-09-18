#include "MaterialResource.h"

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

MaterialResource* MaterialResource::Create(const char* _path)
{
	File f;
	if (!FileSystem::Read(f, _path)) {
		return nullptr;
	}

	MaterialResource* ret = APT_NEW(MaterialResource());
	ret->m_path = _path;
	if (FileSystem::CompareExtension("json", _path)) {
		Json json;
		Json::Read(json, f);
		SerializerJson serializer(json, SerializerJson::Mode_Read);
		APT_VERIFY(Serialize(serializer, *ret));

	} else {
		APT_ASSERT(false); // only json implemented

	}

	return ret;
}

void MaterialResource::Release(MaterialResource* _res)
{
	APT_DELETE(_res);
}


bool Serialize(Serializer& _serializer_, MaterialResource& _res_)
{	
	APT_ASSERT(_serializer_.getMode() == Serializer::Mode_Read); // \todo implement write
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		APT_ASSERT(_res_.m_passList.empty()); // already serialized?

	 // \todo this validation could be moved into common code
		String<32> className;
		APT_VERIFY(Serialize(_serializer_, className, "_class"));
		if (className != "MaterialResource") {
			APT_LOG_ERR("MaterialResource (%s): invalid file '%s'.", FileSystem::StripPath(_res_.m_path.c_str()));
			return false;
		}
		int version;
		APT_VERIFY(Serialize(_serializer_, version, "_version"));
		if (version > MaterialResource::kVersion) {
			APT_LOG_ERR("MaterialResource (%s): incompatible version %d (min is %d).", FileSystem::StripPath(_res_.m_path.c_str()), version, MaterialResource::kVersion);
			return false;
		}

		MaterialResource::PassData def;
		MaterialResource::Serialize(_serializer_, def);
	
		uint passCount = 0;
		if (_serializer_.beginArray(passCount, "pass_list")) {
			_res_.m_passList.reserve(passCount);
		
			while (_serializer_.beginObject()) {
				_res_.m_passList.push_back();
				auto& pass = _res_.m_passList.back();
				Serialize(_serializer_, pass.first, "pass_filters");
			}

			_serializer_.endArray();
		}

	}

	return true;
}

// PRIVATE

MaterialResource::MaterialResource()
{
}

MaterialResource::~MaterialResource()
{
}

bool MaterialResource::Serialize(Serializer& _serializer_, PassData& _passData_)
{
	typedef String<32> Str;

	APT_ASSERT(_serializer_.getMode() == Serializer::Mode_Read);

	bool ret = true;

	if (_serializer_.beginObject("shader")) {
		PathStr path;
		eastl::vector<Str> defines;
		APT_VERIFY(_serializer_.value(path, "path"));
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

		_serializer_.endObject();
	}

	if (_serializer_.beginObject("states")) {

		_serializer_.endObject();
	} else {
		Str state;
		if (_serializer_.value(state, "states")) {
		}
	}

	if (_serializer_.beginObject("textures")) {
		_serializer_.endObject();
	}

	return true;
}