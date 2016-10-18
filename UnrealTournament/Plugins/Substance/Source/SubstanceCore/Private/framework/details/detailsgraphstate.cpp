//! @file detailsgraphstate.cpp
//! @brief Substance Framework Graph Instance State implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceFPackage.h"
#include "SubstanceInput.h"

#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailsgraphbinary.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailslinkdata.h"
#include "framework/details/detailsdeltastate.h"

#include <algorithm>

namespace Substance
{
namespace DetailsGraph
{

static uint32 gInstanceUid = 0x0;

}   // namespace Details
}   // namespace Substance


//! Create from graph instance
//! @param graphInstance The source graph instance, initialize state
//! @note Called from user thread 
Substance::Details::GraphState::GraphState(
		FGraphInstance* graphInstance) :
	mUid((++DetailsGraph::gInstanceUid)|0x80000000u),
	mLinkData(graphInstance->Desc->Parent->getLinkData()),
	mGraphBinary(new GraphBinary(graphInstance))
{
	mInputStates.reserve(graphInstance->Inputs.size());
	
	Substance::List<TSharedPtr<input_inst_t>>::TIterator 
		ItIn(graphInstance->Inputs.itfront());

	for (;ItIn;++ItIn)
	{
		mInputStates.resize(mInputStates.size()+1);
		InputState &Inpst = mInputStates.back();
		Inpst.flags = ItIn->Get()->Desc->Type;
		
		// Initialize from default
		Inpst.initValue(ItIn->Get()->Desc);
	}
	
	mImageInputFirstNullPtr = mInputImagePtrs.size();
}
	
	
//! @brief Destructor
Substance::Details::GraphState::~GraphState()
{
	delete mGraphBinary;
}


//! @brief Apply delta state to current state
void Substance::Details::GraphState::apply(const DeltaState& deltaState)
{
	SBS_VECTOR_FOREACH (const DeltaState::Input& inpsrc,deltaState.getInputs())
	{
		check(inpsrc.index<mInputStates.size());
		const InputState &inpsrcmod = inpsrc.modified;
		
		if (!inpsrcmod.isCacheOnly())
		{
			InputState &inpdst = mInputStates[inpsrc.index];
			if (inpsrcmod.isImage())
			{
				// Set input image
				if (inpdst.value.imagePtrIndex!=InputState::invalidIndex)
				{
					// Remove previous image pointer
					remove(inpdst.value.imagePtrIndex);
				}
					
				const size_t inpind = inpsrcmod.value.imagePtrIndex;

				inpdst.value.imagePtrIndex = inpind!=InputState::invalidIndex ? 
					store(deltaState.getImageInput(inpind)) :
					InputState::invalidIndex;			
			}
			else
			{
				// Set input numeric
				inpdst.value = inpsrcmod.value;
			}
		}
	}
}


//! @brief This state is currently linked (present in current SBSBIN data)
bool Substance::Details::GraphState::isLinked() const
{
	return mGraphBinary->isLinked();
}


//! @brief Store new image pointer
//! @return Return image pointer index in mInputImagePtrs array
size_t Substance::Details::GraphState::store(const ImageInputPtr& imgPtr)
{
	const size_t posres = mImageInputFirstNullPtr;

	if (mImageInputFirstNullPtr>=mInputImagePtrs.size())
	{
		check(mImageInputFirstNullPtr==mInputImagePtrs.size());
		++mImageInputFirstNullPtr;
		mInputImagePtrs.push_back(imgPtr);
	}
	else
	{
		check(mInputImagePtrs[mImageInputFirstNullPtr].get()==NULL);
		mInputImagePtrs[mImageInputFirstNullPtr] = imgPtr;
		
		// Find next free cell
		do
		{
			++mImageInputFirstNullPtr;
		}
		while (mImageInputFirstNullPtr<mInputImagePtrs.size() && 
			mInputImagePtrs[mImageInputFirstNullPtr].get()!=NULL);
	}
	
	return posres;
}


//! @brief Remove image pointer
//! @param Index of the image pointer to remove
void Substance::Details::GraphState::remove(size_t index)
{
	check(mInputImagePtrs[index].get()!=NULL);
	mInputImagePtrs[index].reset();
	mImageInputFirstNullPtr = std::min<size_t>(mImageInputFirstNullPtr,index);
}

