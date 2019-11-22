#pragma once

#include <frm/core/AppSample.h>

class BlockCompression: public frm::AppSample
{
	typedef AppSample AppBase;
public:
	BlockCompression();
	virtual ~BlockCompression();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	frm::PathStr  m_txSrcPath;                      // Path for m_txSrc.
	frm::Texture* m_txSrc          = nullptr;       // Source texture.
	frm::Image*   m_imgSrc         = nullptr;       // CPU-side copy of m_txSrc.
	frm::Shader*  m_shView         = nullptr;       // Shader for final preview.
	frm::ivec2    m_previewRectXY  = frm::ivec2(0); // Position of the preview sub rectangle in texels.

	enum Mode_
	{
		Mode_None,      // Show the final output.
		Mode_Source,    // Show the source image.
		Mode_Error,     // Show the difference with the source image.
		Mode_BlockEp0,  // Show endpoint 0 for each block.
		Mode_BlockEp1,  // show endpoint 1 for each block.

		Mode_Count
	};
	typedef int Mode;
	Mode m_mode = Mode_None;

	struct Test
	{
	 // Resources
		frm::Buffer*  m_bfDst      = nullptr; // Output buffer for the compression shader.
		frm::Texture* m_txDst      = nullptr; // Final compressed texture (copied from m_bfDst).
		frm::Shader*  m_shBc1      = nullptr; // Compression shader (compiled with options, see below).

	 // Options
		bool          m_usePCA     = true;    // Use principle component analysis to find the block enpoints (else use color space extents). 
	};
	Test m_tests[2];

	bool initSourceTexture();
	void shutdownSourceTexture();

	bool initTextures(Test& _test_);
	void shutdownTextures(Test& test_);

	bool initShaders(Test& _test_);
	void shutdownShaders(Test& test_); 

	void drawPreview(int _x, int _y, int _w, int _h, Test& _test);

	void getSourceBlock(frm::Image* _img, int _x, int _y, frm::uint8 block_[(4* 4) * 4]); 
};
