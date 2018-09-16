#pragma once
#ifndef BlockCompression_h
#define BlockCompression_h

#include <frm/core/AppSample.h>

class BlockCompression: public frm::AppSample
{
	typedef AppSample AppBase;
public:
	BlockCompression();
	virtual ~BlockCompression();

	virtual bool init(const apt::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	apt::PathStr  m_txSrcPath;   // path for m_txSrc and m_txCmp
	frm::Texture* m_txSrc;       // source image
	frm::Buffer*  m_bfDst;       // output of the compute shader
	frm::Texture* m_txDst;       // final compressed texture (copied from m_bfDst)
	frm::Texture* m_txCmp;       // offline compressed BC1 for comparison
	frm::Shader*  m_shBc1;
	frm::Shader*  m_shView;

	enum Mode 
	{
		Mode_Node,
		Mode_Difference,

		Mode_Count
	};
	int m_mode;

	bool initTextures();
	void shutdownTextures();

	void drawPreview(int _x, int _y, int _w, int _h, frm::Texture* _txSrc, frm::Texture* _txCmp);
};


#endif // BlockCompression_h
