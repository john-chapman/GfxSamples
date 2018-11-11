#pragma once

#include <frm/core/AppSample3d.h>

using namespace frm;
using namespace apt;

typedef AppSample3d AppBase;

class PCA: public AppBase
{
public:
	PCA();
	virtual ~PCA();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
	int    m_dataCount               = 16;
	int    m_randomSeed              = 1234;
	vec3*  m_rawData                 = nullptr;
	vec3*  m_data                    = nullptr;
	mat4   m_worldMatrix             = identity;
	bool   m_showGizmo               = true;
	bool   m_incrementalEstimateAvg  = false;

	vec3 pcaBatch(const vec3& _min, const vec3& _max, const vec3& _avg);
	vec3 pcaIncremental(const vec3& _min, const vec3& _max, vec3& _avg_);
	void selectEndpoints(const vec3& _axis, const vec3& _avg, vec3& ep0_, vec3& ep1_);

	void initData();
	void shutdownData();
	
};
