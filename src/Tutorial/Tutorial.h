#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class Tutorial: public AppBase
{
public:
	Tutorial();
	virtual ~Tutorial();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
};
