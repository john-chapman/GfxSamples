#include "MeshResource.h"

#include "MaterialResource.h"

#include <frm/core/Mesh.h>

#include <apt/log.h>
#include <apt/memory.h>
#include <apt/FileSystem.h>
#include <apt/Json.h>
#include <apt/Serializer.h>

using namespace frm;
using namespace apt;

// PUBLIC

MeshResource* MeshResource::Create(const char* _path)
{
	File f;
	if (!FileSystem::Read(f, _path)) {
		return nullptr;
	}

	MeshResource* ret = APT_NEW(MeshResource());
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

void MeshResource::Release(MeshResource* _res)
{
	APT_DELETE(_res);
}

bool Serialize(Serializer& _serializer_, MeshResource& _res_)
{
 // \todo better error handling in this function, can skip invalid meshes or materials without assert, only fully empty resources are illegal

	APT_ASSERT(_serializer_.getMode() == Serializer::Mode_Read); // \todo implement write
	if (_serializer_.getMode() == Serializer::Mode_Read) {
		APT_ASSERT(_res_.m_passList.empty()); // already serialized?

	 // \todo this validation could be moved into common code
		String<32> className;
		APT_VERIFY(Serialize(_serializer_, className, "_class"));
		if (className != "MeshResource") {
			APT_LOG_ERR("MeshResource (%s): invalid file '%s'.", FileSystem::StripPath(_res_.m_path.c_str()));
			return false;
		}
		int version;
		APT_VERIFY(Serialize(_serializer_, version, "_version"));
		if (version > MeshResource::kVersion) {
			APT_LOG_ERR("MeshResource (%s): incompatible version %d (min is %d).", FileSystem::StripPath(_res_.m_path.c_str()), version, MeshResource::kVersion);
			return false;
		}

	// \todo break this up into separate functions

		if (_serializer_.beginObject("lod_coefficients")) {
			Serialize(_serializer_, _res_.m_lodCoefficients.m_size,         "size");
			Serialize(_serializer_, _res_.m_lodCoefficients.m_distance,     "distance");
			Serialize(_serializer_, _res_.m_lodCoefficients.m_eccentricity, "eccentricity");
			Serialize(_serializer_, _res_.m_lodCoefficients.m_velocity,     "velocity");
			_serializer_.endObject();
		}

		uint passCount = 0;
		if (_serializer_.beginArray(passCount, "pass_list")) {
			_res_.m_passList.reserve(passCount);

			while (_serializer_.beginObject()) {
				_res_.m_passList.push_back();
				auto& pass = _res_.m_passList.back();
				Serialize(_serializer_, pass.first, "pass_filters");

				auto& lodList  = pass.second;
				uint lodCount = 0;
				if (_serializer_.beginArray(lodCount, "lod_list")) {
					lodList.reserve(lodCount);
					while (_serializer_.beginObject()) {
						lodList.push_back();
						auto& lod = lodList.back();
	
					 // Mesh
						PathStr meshPath;
						if (!Serialize(_serializer_, meshPath, "mesh")) {
							APT_LOG_ERR("MeshResource (%s): failed to serialize mesh path (pass list %d, lod %d)", FileSystem::StripPath(_res_.m_path.c_str()), (int)_res_.m_passList.size() - 1, (int)lodList.size() - 1);
						}
						lod.m_mesh = Mesh::Create(meshPath.c_str()); // calls Use() implicitly
	
					 // Material list
						uint matCount = 0;
						if (_serializer_.beginArray(matCount, "material_list")) {
							APT_ASSERT((int)matCount <= lod.m_mesh->getSubmeshCount());
							PathStr matPath;
							while (Serialize(_serializer_, matPath)) {
								lod.m_materials.push_back();
								lod.m_materials.back() = MaterialResource::Create(matPath.c_str());
							}
							_serializer_.endArray(); // material_list
						}
						_serializer_.endObject(); // lod
					}
					_serializer_.endArray(); // lod_list
				}
				_serializer_.endObject(); // pass
			}
			_serializer_.endArray(); // pass_list
		}

	}
	return true;
}

// PRIVATE

MeshResource::MeshResource()
{
}

MeshResource::~MeshResource()
{
	for (auto& passList : m_passList) {
		for (auto& lodData : passList.second) {
			Mesh::Release(lodData.m_mesh);
			for (auto& mat : lodData.m_materials) {
				MaterialResource::Release(mat);
			}
		}
		passList.second.clear();
	}
	m_passList.clear();
}
