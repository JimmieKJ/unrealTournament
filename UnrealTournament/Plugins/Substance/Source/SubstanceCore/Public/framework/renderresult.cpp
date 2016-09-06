//! @file renderresult.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "framework/renderresult.h"
#include "framework/details/detailsengine.h"


Substance::RenderResult::RenderResult(
		const SubstanceTexture& texture, 
		SubstanceContext* context, 
		Details::Engine* engine):
	mSubstanceTexture(texture),
	mHaveOwnership(true),
	mContext(context),
	mEngine(engine)
{
	check(mEngine!=NULL);
}


Substance::RenderResult::~RenderResult()
{
	if (mHaveOwnership)
	{
		mEngine->enqueueRelease(mSubstanceTexture);
	}
}


//! @brief Grab the pixel data buffer, the pointer is set to NULL
//! @warning The ownership of the buffer is transferred to the caller.
//! 	The buffer must be free by substanceContextMemoryFree(), see
//!		substance/context.h for further information.
//! @return The buffer, or NULL if already released
SubstanceTexture Substance::RenderResult::releaseTexture()
{
	check(haveOwnership());
	mHaveOwnership = false;
	return mSubstanceTexture;
}
