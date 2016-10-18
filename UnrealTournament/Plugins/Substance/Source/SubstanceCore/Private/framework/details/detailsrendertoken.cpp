//! @file detailsrendertoken.cpp
//! @brief Substance rendering result token
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"

#include "framework/renderresult.h"
#include "framework/details/detailsrendertoken.h"
#include "framework/details/detailssync.h"
#include "framework/details/detailsengine.h"


//! @brief Constructor
Substance::Details::RenderToken::RenderToken() :
	mRenderResult(NULL),
	mFilled(false),
	mPushCount(1),
	mRenderCount(1)
{
}


//! @brief Destructor
//! Delete render result if present
Substance::Details::RenderToken::~RenderToken()
{
	if (mRenderResult!=NULL)
	{
		clearRenderResult(mRenderResult);
	}
}


//! @brief Fill render result (grab ownership)
void Substance::Details::RenderToken::fill(RenderResult* renderResult)
{
	RenderResult* prevrres = (RenderResult*)Sync::interlockedSwapPointer(
		(void*volatile*)&mRenderResult,
		renderResult);

	mFilled = true;              // do not change affectation order
	
	if (prevrres!=NULL)
	{
		clearRenderResult(prevrres);
	}
}


//! @brief Increment push and render counter
void Substance::Details::RenderToken::incrRef()
{
	++mPushCount;
	++mRenderCount;
}


//! @brief Decrement render counter
void Substance::Details::RenderToken::canceled()
{
	check(mRenderCount>0);
	--mRenderCount;
}


//! @brief Return true if can be removed from OutputInstance queue
//! Can be removed if NULL filled or push count is >1 or render count ==0
//! @post Decrement push counter
bool Substance::Details::RenderToken::canRemove()
{
	if ((mFilled && mRenderResult==NULL) ||    // do not change && order
		mPushCount>1 || 
		mRenderCount==0)
	{
		check(mPushCount>0);
		--mPushCount;
		return true;
	}
	
	return false;
}


//! @brief Return render result or NULL if pending, transfer ownership
//! @post mRenderResult becomes NULL
Substance::RenderResult* Substance::Details::RenderToken::grabResult()
{
	RenderResult* res = NULL;
	if (mRenderResult!=NULL && mRenderCount>0)
	{
		res = (RenderResult*)Sync::interlockedSwapPointer(
			(void*volatile*)&mRenderResult,
			NULL);
	}
	
	return res;
}


//! @brief Delete render results w/ specific engine UID
bool Substance::Details::RenderToken::releaseOwnedByEngine(uint32 engineUid)
{
	if (mRenderResult!=NULL && 
		mRenderResult->getEngine()->getInstanceUid()==engineUid)
	{
		clearRenderResult(mRenderResult);
		return true;
	}

	return false;
}


void Substance::Details::RenderToken::clearRenderResult(
	RenderResult* renderResult)
{
	check(renderResult->haveOwnership());

	renderResult->getEngine()->enqueueRelease(renderResult->releaseTexture());
}
