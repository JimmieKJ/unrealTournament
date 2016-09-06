//! @file detailscomputation.cpp
//! @brief Substance Framework Computation action implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111116
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"
#include "framework/details/detailscomputation.h"
#include "framework/details/detailsengine.h"
#include "framework/details/detailsinputstate.h"
#include "framework/details/detailsinputimagetoken.h"

#include <algorithm>
#include <iterator>
	
//! @brief Constructor from engine
//! @param flush Flush internal engine queue if true
//! @pre Engine is valid and correctly linked
//! @post Engine render queue is flushed
Substance::Details::Computation::Computation(Engine& engine, bool flush) :
	mEngine(engine)
{
	check(mEngine.getHandle()!=NULL);
	if (flush)
	{
		mEngine.flush();
	}
}


//! @brief Push input
//! @param index SBSBIN index
//! @param inputState Input type, value and other flags
//! @param imgToken Image token pointer, used if input image
//! @return Return if input really pushed
bool Substance::Details::Computation::pushInput(
	uint32 index,
	const InputState& inputState,
	ImageInputToken* imgToken)
{
	bool res = false;
	check(inputState.isImage() || imgToken==NULL);
	
	if (!inputState.isCacheOnly())
	{
		// Push input value
		int err = Substance::gAPI->SubstanceHandlePushSetInput(
			mEngine.getHandle(),
			index,
			inputState.getType(),
			inputState.isImage() ?
				(imgToken != NULL ? &(imgToken->texture) : NULL) :
				const_cast<void*>((const void*)inputState.value.numeric),
			mUserData);

		if (err)
		{
			// TODO2 Warning: push input failed
			check(0);
		}
		else
		{
			res = true;
		}
	}
	
	if (inputState.isCacheEnabled())
	{
		// To push as hint
		mHintInputs.push_back(getHintInput(inputState.getType(),index));
	}
	
	return res;
}

	
//! @brief Push outputs SBSBIN indices to compute 
void Substance::Details::Computation::pushOutputs(const Indices& indices)
{
	int err = Substance::gAPI->SubstanceHandlePushOutputs(
		mEngine.getHandle(),
		&indices[0],
		indices.size(),
		mUserData);
		
	if (err)
	{
		// TODO2 Warning: pust outputs failed
		check(0);
	}
	
	// To push as hint
	mHintOutputs.insert(mHintOutputs.end(),indices.begin(),indices.end());
}

	
//! @brief Run computation
//! Push hints I/O and run
void Substance::Details::Computation::run()
{
	SubstanceHandle* handle = mEngine.getHandle();
	
	if (!mHintInputs.empty())
	{
		// Make unique
		std::sort(mHintInputs.begin(),mHintInputs.end());
		mHintInputs.erase(std::unique(
			mHintInputs.begin(),mHintInputs.end()),mHintInputs.end());
	}

	SBS_VECTOR_FOREACH (uint32 hintinp,mHintInputs)
	{
		// Push input hint
		int err = Substance::gAPI->SubstanceHandlePushSetInputHint(
			handle,
			getIndex(hintinp),
			getType(hintinp));
			
		if (err)
		{
			// TODO2 Warning: push input failed
			check(0);
		}
	}
	
	if (!mHintOutputs.empty())
	{
		// Make unique
		std::sort(mHintOutputs.begin(),mHintOutputs.end());
		const size_t newsize = std::distance(mHintOutputs.begin(),
			std::unique(mHintOutputs.begin(),mHintOutputs.end()));
	
		// Push output hints
		unsigned int err = Substance::gAPI->SubstanceHandlePushOutputsHint(
			handle,
			&mHintOutputs[0],
			newsize);
			
		if (err)
		{
			// TODO2 Warning: push outputs failed
			check(0);
		}
	}
	
	// Start computation
	int err = Substance::gAPI->SubstanceHandleCompute(handle);
	
	if (err)
	{
		// TODO2 Error: start failed
		check(0);
	}
}

