#pragma once

#include <frm/core/def.h>

#include <apt/String.h>

#include <EASTL/fixed_vector.h>

class MaterialResource;

// https://pdfs.semanticscholar.org/presentation/2b40/34c1638aa8c24324b508d80ad14dab0511e3.pdf
struct LodCoefficients
{
	float m_size           = 0.0f; // Coeffient for projected size metric.
	float m_distance       = 0.0f; //      "        distance.
	float m_eccentricity   = 0.0f; //      "        eccentricity (in periphery vision).
	float m_velocity       = 0.0f; //      "        velocity.
};

///////////////////////////////////////////////////////////////////////////////
// MeshResource
// Mesh/material pairs, grouped into order lists for LOD selection.
// Multiple LOD lists corresponding to pass masks may be present (e.g. to
// support proxy meshes for shadow passes).
//
// \todo LodCoefficients per pass?
// \todo Resolve pass masks at load time to avoid string comparisons.
///////////////////////////////////////////////////////////////////////////////
class MeshResource
{
public:
	static constexpr int kVersion = 0;

	// Mesh + per submesh material resource. Note that submesh 0 represents all
	// submeshes, hence element 0 in the material list may be null.
	struct LodData
	{
		frm::Mesh* m_mesh;
		eastl::fixed_vector<MaterialResource*, 1> m_materials;
	};

	static MeshResource* Create(const char* _path);
	static void Release(MeshResource* _res);

	int findPass(const char* _name) const;
	
	const LodData& getLod(int _pass, int _lod) const    { return m_passList[_pass].second[_lod]; }
	const LodCoefficients& getLodCoefficients() const   { return m_lodCoefficients; }

	friend bool Serialize(apt::Serializer& _serializer_, MeshResource& _res_);

private:
	typedef eastl::fixed_vector<LodData, 5> LodList;
	
	apt::PathStr m_path;
	eastl::fixed_vector<eastl::pair<apt::String<8>, LodList>, 1> m_passList;
	LodCoefficients m_lodCoefficients;

	MeshResource();
	~MeshResource();
	
};

bool Serialize(apt::Serializer& _serializer_, MeshResource& _res_);

inline int MeshResource::findPass(const char* _name) const
{
 // first LodList is the 'default', start the search at 1
	for (int i = 1, n = (int)m_passList.size(); i < n; ++i) {
		if (m_passList[i].first == _name) { // \todo could me more complex e.g. a comma separated list
			return i;
		}
	} 

	return 0;
}