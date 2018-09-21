#include "Model.h"

#include "Material.h"

#include <frm/core/Mesh.h>

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>
#include <apt/Json.h>
#include <apt/Serializer.h>

using namespace frm;
using namespace apt;

// PUBLIC

Model* Model::Create(const char* _path)
{
	File f;
	if (!FileSystem::Read(f, _path)) {
		return nullptr;
	}

	Model* ret = APT_NEW(Model());
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

void Model::Release(Model* _res)
{
	APT_DELETE(_res);
}

bool Serialize(Serializer& _serializer_, Model& _res_)
{
 // \todo better error handling in this function, can skip invalid meshes or materials without assert, only fully empty resources are illegal

	APT_ASSERT(_serializer_.getMode() == Serializer::Mode_Read); // \todo implement write
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		APT_ASSERT(_res_.m_passList.empty()); // already serialized?

	 // \todo this validation could be moved into common code
		String<32> className;
		APT_VERIFY(Serialize(_serializer_, className, "_class"));
		if (className != "Model") {
			APT_LOG_ERR("Model (%s): invalid file '%s'.", FileSystem::StripPath(_res_.m_path.c_str()));
			return false;
		}
		int version;
		APT_VERIFY(Serialize(_serializer_, version, "_version"));
		if (version > Model::kVersion) {
			APT_LOG_ERR("Model (%s): incompatible version %d (min is %d).", FileSystem::StripPath(_res_.m_path.c_str()), version, Model::kVersion);
			return false;
		}

		LodCoefficients defaultLodCoefficients;
		Model::Serialize(_serializer_, defaultLodCoefficients);

		if (_serializer_.beginObject("pass_list")) {
			while (_serializer_.beginObject()) {
				_res_.m_passList.push_back();
				auto& pass = _res_.m_passList.back();
				pass.first = _serializer_.getName();
				pass.second.m_lodCoefficients = defaultLodCoefficients;
				Model::Serialize(_serializer_, pass.second);
				_serializer_.endObject(); // pass
			}
			_serializer_.endObject(); // pass_list
		}

	}
	return true;
}

// PRIVATE

bool Model::Serialize(Serializer& _serializer_, LodCoefficients& _lodCoefficients_)
{
	if (!_serializer_.beginObject("lod_coefficients")) {
		return false;
	}
	_serializer_.value(_lodCoefficients_.m_size,         "size");
	_serializer_.value(_lodCoefficients_.m_distance,     "distance");
	_serializer_.value(_lodCoefficients_.m_eccentricity, "eccentricity");
	_serializer_.value(_lodCoefficients_.m_velocity,     "velocity");
	_serializer_.endObject();
	
	return true;
}

bool Model::Serialize(Serializer& _serializer_, PassData& _passData_)
{
	uint lodCount;
	if (!_serializer_.beginArray(lodCount, "lod_list")) {
		return false;
	}
	_passData_.m_lodList.reserve(lodCount);
	while (_serializer_.beginObject()) {
		_passData_.m_lodList.push_back();
		auto& lod = _passData_.m_lodList.back();
	
		PathStr meshPath;
		apt::Serialize(_serializer_, meshPath, "mesh");
meshPath.append(".obj"); // \todo
		lod.m_mesh = Mesh::Create(meshPath.c_str()); // calls Use() implicitly

		uint matCount = 0;
		if (_serializer_.beginArray(matCount, "material_list")) {
			APT_ASSERT((int)matCount <= lod.m_mesh->getSubmeshCount());
			PathStr matPath;
			while (_serializer_.value(matPath)) {
matPath.append(".json"); // \todo
				lod.m_materials.push_back();
				lod.m_materials.back() = Material::Create(matPath.c_str());
			}
			_serializer_.endArray(); // material_list
		}

		_serializer_.endObject(); // lod
	}
			
	_serializer_.endArray(); // lod_list

	Serialize(_serializer_, _passData_.m_lodCoefficients);

	return true;
}

Model::Model()
{
}

Model::~Model()
{
	for (auto& passList : m_passList) {
		for (auto& lodData : passList.second.m_lodList) {
			Mesh::Release(lodData.m_mesh);
			for (auto& mat : lodData.m_materials) {
				Material::Release(mat);
			}
		}
		passList.second.m_lodList.clear();
	}
	m_passList.clear();
}
