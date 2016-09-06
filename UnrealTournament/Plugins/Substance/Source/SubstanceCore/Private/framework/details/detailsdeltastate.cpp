//! @file detailsdeltastate.cpp
//! @brief Substance Framework Instance Inputs Delta State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"
#include "framework/details/detailsdeltastate.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailsinputstate.h"

#include "SubstanceFGraph.h"

#include <algorithm>
#include <iterator>

	
//! @brief Fill a delta state from current state & current instance values
//! @param graphState The current graph state used to generate delta
//! @param graphInstance The pushed graph instance (not keeped)
void Substance::Details::DeltaState::fill(
	const GraphState &graphState,
	const FGraphInstance* graphInstance)
{
	size_t inpindex = 0;
	Substance::List<TSharedPtr<input_inst_t>>::TConstIterator
		ItIn(graphInstance->Inputs.itfrontconst());
	
	for	(;ItIn;++ItIn)
	{
		const InputState &inpst = graphState[inpindex];

		check((*ItIn).Get()->Desc->Type==inpst.getType());
		const bool ismod = (*ItIn).Get()->isModified(inpst.value.numeric);
		
		if (ismod || (*ItIn).Get()->UseCache)
		{
			// Different or cache forced, create delta entry
			mInputs.resize(mInputs.size()+1);
			Input &inpdelta = mInputs.back();
			inpdelta.index = inpindex;
			inpdelta.modified.flags = inpst.getType();
			inpdelta.previous = inpst;
			
			if (inpst.isImage() && 
				inpst.value.imagePtrIndex!=InputState::invalidIndex)
			{
				// Save previous pointer
				inpdelta.previous.value.imagePtrIndex = mImageInputPtrs.size();
				mImageInputPtrs.push_back(
					graphState.getInputImage(inpst.value.imagePtrIndex));
			}
			
			if (ismod)
			{
				// Really modified, 
				inpdelta.modified.fillValue(ItIn->Get(), mImageInputPtrs);
			}
			else
			{
				// Only for cache
				inpdelta.modified.value = inpdelta.previous.value;
				inpdelta.modified.flags |= InputState::Flag_CacheOnly;
			}
			
			if (!ItIn->Get()->IsHeavyDuty)
			{
				inpdelta.modified.flags |= InputState::Flag_Cache;
			}
		}
		
		++inpindex;
	}
}


namespace Substance
{
	namespace Details
	{

		template <>
		bool DeltaState::isEqual(
			const InputState& a,
			const InputState& b,
			const DeltaState &bparent) const
		{
			check(a.isImage() == b.isImage());
			if (b.isImage())
			{
				// Image pointer equality
				const bool bvalid = b.value.imagePtrIndex != InputState::invalidIndex;
				if (bvalid != (a.value.imagePtrIndex != InputState::invalidIndex))
				{
					return false;          // One valid, not the other
				}

				return !bvalid ||
					mImageInputPtrs[a.value.imagePtrIndex] ==
					bparent.mImageInputPtrs[b.value.imagePtrIndex];
			}
			else
			{
				// Numeric equality
				return std::equal(
					b.value.numeric,
					b.value.numeric + getComponentsCount(b.getType()),
					a.value.numeric);
			}
		}


		template <>
		bool DeltaState::isEqual(
			const OutputState& a,
			const OutputState& b,
			const DeltaState &) const
		{
			return a.formatOverridden == b.formatOverridden &&
				(!a.formatOverridden || a.formatOverride == b.formatOverride);
		}


		template <>
		void DeltaState::record(
			InputState& inputState,
			const DeltaState &parent)
		{
			const size_t srcindex = inputState.value.imagePtrIndex;
			if (inputState.isImage() && srcindex != InputState::invalidIndex)
			{
				inputState.value.imagePtrIndex = mImageInputPtrs.size();
				mImageInputPtrs.push_back(parent.mImageInputPtrs[srcindex]);
			}
		}


		template <>
		void DeltaState::record(
			OutputState&,
			const DeltaState&)
		{
			// do nothing
		}

	}  // namespace Substance
}  // namespace Details


template <typename Items>
void Substance::Details::DeltaState::append(
	Items& destItems,
	const Items& srcItems,
	const DeltaState &src,
	AppendMode mode)
{
	typename Items::const_iterator previte = destItems.begin();
	Items newitems;
	newitems.reserve(srcItems.size() + destItems.size());
	
	for (auto srcitem = srcItems.begin(); srcitem != srcItems.end(); ++srcitem)
	{
		while (previte!=destItems.end() && previte->index<srcitem->index)
		{
			// Copy directly non modified items
			newitems.push_back(*previte);
			++previte;
		}
		
		if (previte!=destItems.end() && previte->index == srcitem->index)
		{
			// Collision
			switch (mode)
			{
				case Append_Default:
					newitems.push_back(*previte);   // Existing used
				break;
				
				case Append_Reverse:
					newitems.push_back(*previte);    // Existing used for modified
					newitems.back().previous = srcitem->modified; // Update previous
					record(newitems.back().previous,src);
				break;
				
				case Append_Override:
					if (!isEqual(previte->previous,srcitem->modified,src))
					{
						// Combine only if no identity
						newitems.push_back(*previte); // Existing used for previous
						newitems.back().modified = srcitem->modified; // Update modified
						record(newitems.back().modified,src);
					}
				break;
			}
			
			++previte;
		}
		else
		{
			// Insert (into newitems)
			newitems.push_back(*srcitem);
			if (mode==Append_Reverse)
			{
				std::swap(newitems.back().previous,newitems.back().modified);
			}

			record(newitems.back().modified,src);
			record(newitems.back().previous,src);
		}
	}

	std::copy(
		previte,
		const_cast<const Items&>(destItems).end(),
		std::back_inserter(newitems));    // copy remains
	destItems = newitems;                   // replace inputs
}


//! @brief Append a delta state to current
//! @param src Source delta state to append
//! @param mode Append policy flag
void Substance::Details::DeltaState::append(
	const DeltaState &src,
	AppendMode mode)
{
	append<Inputs>(mInputs,src.getInputs(),src,mode);
	append<Outputs>(mOutputs,src.getOutputs(),src,mode);
}
