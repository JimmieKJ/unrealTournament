//! @file detailsrendertoken.h
//! @brief Substance rendering result token
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERTOKEN_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERTOKEN_H

#include "framework/renderresult.h"

namespace Substance
{

namespace Details
{

//! @brief Substance rendering result shell struct
//! Queued in OutputInstance
class RenderToken
{
public:

	//! @brief Constructor
	RenderToken();

	//! @brief Destructor
	//! Delete render result if present
	~RenderToken();

	//! @brief Fill render result (grab ownership)
	void fill(RenderResult* renderResult);
	
	//! @brief Increment push and render counter
	void incrRef();
	
	//! @brief Decrement render counter
	void canceled();
	
	//! @brief Return true if can be removed from OutputInstance queue
	//! Can be removed if NULL filled or push count is >1 or render count ==0
	//! @post Decrement push counter
	bool canRemove();
	
	//! @brief Return if already computed
	bool isComputed() const { return mFilled; }
	
	//! @brief Return render result or NULL if pending, transfer ownership
	//! @post mRenderResult becomes NULL
	RenderResult* grabResult();

	//! @brief Delete render results w/ specific engine UID
	//! @param engineUid The UID of engine to delete render result
	//! @return Return true if render result is effectively released
	bool releaseOwnedByEngine(uint32 engineUid);
	
protected:

	//! @brief The pointer on render result or NULL if grabbed/skipped/pending
	RenderResult *volatile mRenderResult;

	//! @brief True if filled, can be removed in OutputInstance
	volatile bool mFilled;
	
	//! @brief Pushed in OutputInstance count
	size_t mPushCount;
	
	//! @brief Render pending count (not canceled)
	size_t mRenderCount;

	//! @brief Delete render result
	static void clearRenderResult(RenderResult* renderResult);

private:
	RenderToken(const RenderToken&);
	const RenderToken& operator=(const RenderToken&);

};  // class RenderToken


} // namespace Details
} // namespace Substance

#endif //_SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERTOKEN_H
