//! @file detailsgraphbinary.cpp
//! @brief Substance Framework Graph binay information implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceFGraph.h"

#include "framework/details/detailsgraphbinary.h"

#include <algorithm>

//! @brief Invalid index
const uint32 Substance::Details::GraphBinary::invalidIndex = 0xFFFFFFFFu;


//! Create from graph instance
//! @param graphInstance The graph instance, used to fill UIDs
Substance::Details::GraphBinary::GraphBinary(FGraphInstance* graphInstance) :
	mLinked(false)
{
	// Inputs
	{
		inputs.resize(graphInstance->Inputs.size());
		sortedInputs.resize(inputs.size());

		Inputs::iterator eite = inputs.begin();
		InputsPtr::iterator eptrite = sortedInputs.begin();

		Substance::List<TSharedPtr<input_inst_t>>::TIterator
			inpinst(graphInstance->Inputs.itfront());

		for (; inpinst; ++inpinst)
		{
			eite->uidInitial = eite->uidTranslated = (*inpinst)->Desc->Uid;
			eite->index = invalidIndex;
			*(eptrite++) = &*(eite++);
		}
	}
	// Outputs
	{
		outputs.resize(graphInstance->Outputs.size());
		sortedOutputs.resize(outputs.size());

		Outputs::iterator eite = outputs.begin();
		OutputsPtr::iterator eptrite = sortedOutputs.begin();

		Substance::List<output_inst_t>::TIterator
			outinst(graphInstance->Outputs.itfront());

		for (; outinst; ++outinst)
		{
			const output_desc_t* outdesc = (*outinst).GetOutputDesc();
			eite->uidInitial = eite->uidTranslated = (*outinst).GetOutputDesc()->Uid;
			eite->index = invalidIndex;
			eite->enqueuedLink = false;
			eite->descFormat = outdesc->Format;
			//eite->descMipmap = outdesc->mMipmaps;
			//eite->descForced = outdesc.mFmtOverridden;
			*(eptrite++) = &*(eite++);
		}
	}

	// Sort by initial uid
	std::sort(sortedInputs.begin(),sortedInputs.end(),InitialUidOrder());
	std::sort(sortedOutputs.begin(),sortedOutputs.end(),InitialUidOrder());
}


//! Reset translated UIDs before new link process
void Substance::Details::GraphBinary::resetTranslatedUids()
{
	SBS_VECTOR_FOREACH (Entry& entry,inputs)
	{
		entry.uidTranslated = entry.uidInitial;
		entry.index = invalidIndex;
	}
	
	SBS_VECTOR_FOREACH (Entry& entry,outputs)
	{
		entry.uidTranslated = entry.uidInitial;
		entry.index = invalidIndex;
	}
}


//! @brief Notify that this binary is linked
void Substance::Details::GraphBinary::linked()
{
	mLinked = true;
}

