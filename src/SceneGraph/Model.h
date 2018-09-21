#pragma once

#include <frm/core/def.h>

#include <apt/String.h>

#include <EASTL/fixed_vector.h>

class Material;

// https://pdfs.semanticscholar.org/presentation/2b40/34c1638aa8c24324b508d80ad14dab0511e3.pdf
struct LodCoefficients
{
	float m_size           = 0.0f; // Coeffient for projected size metric.
	float m_distance       = 0.0f; //      "        distance.
	float m_eccentricity   = 0.0f; //      "        eccentricity (in periphery vision).
	float m_velocity       = 0.0f; //      "        velocity.
};

///////////////////////////////////////////////////////////////////////////////
// Model
// Mesh/material pairs, grouped into ordered lists for LOD selection.
// Multiple LOD lists corresponding to pass masks may be present (e.g. to
// support proxy meshes for shadow passes).
//
// \todo Resolve pass masks at load time to avoid string comparisons.
///////////////////////////////////////////////////////////////////////////////
class Model
{
public:
	static constexpr int kVersion = 0;

	// Mesh + per submesh material resource. Note that submesh 0 represents all
	// submeshes, hence element 0 in the material list may be null.
	struct LodData
	{
		frm::Mesh* m_mesh;
		eastl::fixed_vector<Material*, 1> m_materials;
	};

	static Model* Create(const char* _path);
	static void Release(Model* _res);

	int findPass(const char* _name) const;
	
	const LodData& getLod(int _pass, int _lod) const           { return m_passList[_pass].second.m_lodList[_lod]; }
	const LodCoefficients& getLodCoefficients(int _pass) const { return m_passList[_pass].second.m_lodCoefficients; }

	friend bool Serialize(apt::Serializer& _serializer_, Model& _res_);

private:
	typedef eastl::fixed_vector<LodData, 5> LodList;
	struct PassData
	{
		LodCoefficients m_lodCoefficients;
		LodList m_lodList;
	};
	
	apt::PathStr m_path;
	eastl::fixed_vector<eastl::pair<apt::String<8>, PassData>, 1> m_passList;

	static bool Serialize(apt::Serializer& _serializer_, LodCoefficients& _lodCoefficients_);
	static bool Serialize(apt::Serializer& _serializer_, PassData& _passData_);

	Model();
	~Model();	
};

bool Serialize(apt::Serializer& _serializer_, Model& _res_);

inline int Model::findPass(const char* _name) const
{
 // first LodList is the 'default', start the search at 1
	for (int i = 1, n = (int)m_passList.size(); i < n; ++i) {
		if (m_passList[i].first == _name) { // \todo could me more complex e.g. a comma separated list
			return i;
		}
	} 

	return 0;
}