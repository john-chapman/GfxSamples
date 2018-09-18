#pragma once

#include <frm/core/AppSample3d.h>

class SceneGraph: public frm::AppSample3d
{
	typedef AppSample3d AppBase;
public:
	SceneGraph();
	virtual ~SceneGraph();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

};

