//! @file detailsinputstate.cpp
//! @brief Substance Framework Input Instance State impl
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!
 
#include "SubstanceCorePrivatePCH.h"
#include "framework/details/detailsinputstate.h"

#include <algorithm>


const size_t Substance::Details::InputState::invalidIndex = 
	(size_t)((ptrdiff_t)-1);


//! @brief Set input state value from instance
//! @param[out] dst Destination input state
//! @param inst Input instance to get value
//! @param[in,out] imagePtrs Array of pointers on ImageInput indexed by 
//!		imagePtrIndex
void Substance::Details::InputState::fillValue(
	const FInputInstanceBase* inst,
	ImageInputPtrs &imagePtrs)
{
	check(getType()==inst->Desc->Type);

	if (!inst->Desc->IsNumerical())
	{
		// Input image
		const ImageInputPtr imgptr = 
			static_cast<const FImageInputInstance*>(inst)->GetImage();

		if (imgptr.get()!=NULL)
		{
			value.imagePtrIndex = imagePtrs.size();
			imagePtrs.push_back(imgptr);
		}
		else
		{
			value.imagePtrIndex = invalidIndex;
		}
	}
	else
	{
		// Input numeric
		const uint32*const newvalue = (const uint32*)(static_cast
			<const FNumericalInputInstanceBase*>(inst)->getRawData());
		std::copy(newvalue,newvalue+getComponentsCount(getType()),value.numeric);
	}
}


//! @brief Initialize value from description default value
//! @param desc Input description to get default value
void Substance::Details::InputState::initValue(input_desc_t* desc)
{
	check(getType()==desc->Type);
	if (desc->IsImage())
	{
		// Input image, NULL
		value.imagePtrIndex = invalidIndex;
	}
	else if (desc->Type==Substance_IType_Integer2 &&
		desc->Identifier == FString(TEXT("$outputsize")))
	{
		// Input numeric, $outputsize case
		// Value may be forced in description
		// Set default to invalid value
		value.numeric[0] = value.numeric[1] = 0xFFFFFFFFu;
	}
	else
	{
		// Input numeric, no $outputsize
		const uint32*const dflvalue = (const uint32*)(desc->getRawDefault());
		std::copy(dflvalue,dflvalue+getComponentsCount(getType()),value.numeric);
	}
}

