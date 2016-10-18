//! @file renderresult.h
//! @brief Substance rendering result
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_RENDERRESULT_H
#define _SUBSTANCE_RENDERRESULT_H

#include "substance_public.h"

namespace Substance
{
namespace Details
{
	class Engine;
}

//! @brief Substance rendering result struct
//! @invariant Result must be deleted or content must be grabbed before
//!		corresponding renderer deletion or renderer engine implementation is 
//!		switch.
struct RenderResult
{
	//! @brief Constructor, internal use only
	RenderResult(const SubstanceTexture&, SubstanceContext*, Details::Engine*);

	//! @brief Destructor
	//! Delete texture content contained in mSubstanceTexture 
	//! if not previously grabbed (releaseBuffer() or releaseTexture()). 
	~RenderResult();

	//! @brief Accessor on the Substance context to use for releasing the 
	//!		texture content.
	SubstanceContext* getContext() const { return mContext; }
	
	//! @brief Grab the pixel data buffer, the pointer is set to NULL
	//! @warning The ownership of the buffer is transferred to the caller.
	//! 	The buffer must be freed by substanceContextMemoryFree(), see
	//!		substance/context.h for further information.
	//! @return Return the buffer, or NULL if already released
	void* releaseBuffer();

	//! @brief Grab texture content ownership
	//! @pre The texture content was not previously grabbed (haveOwnership()==true)
	//! @return Return the texture. The ownership of the texture content 
	//!		is transferred to the caller. 
	SubstanceTexture releaseTexture();
	
	//! @brief Accessor on the result texture
	//! Contains pixel data buffer.
	//! @warning Read-only accessor, do not delete the texture content.
	//! @return Return the texture or invalid texture if renderer is deleted 
	//!		or its engine is switched. 
	const SubstanceTexture& getTexture() const { return mSubstanceTexture; }

	//! @brief Return if the render result still have ownership on content
	bool haveOwnership() const { return mHaveOwnership; }

	//! @brief Internal use
	Details::Engine* getEngine() const { return mEngine; }
	
protected:
	SubstanceTexture mSubstanceTexture;
	bool mHaveOwnership;
	SubstanceContext* mContext;

	Details::Engine* mEngine;

private:
	RenderResult(const RenderResult&);
	const RenderResult& operator=(const RenderResult&);

};

} // namespace Substance

#endif //_SUBSTANCE_RENDERRESULT_H
