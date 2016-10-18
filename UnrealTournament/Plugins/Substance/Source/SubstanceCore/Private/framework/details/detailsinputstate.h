//! @file detailsinputstate.h
//! @brief Substance Framework Input Instance State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111026
//! @copyright Allegorithmic. All rights reserved.
//!
 
#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSINPUTSTATE_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSINPUTSTATE_H

#include <Engine.h>

#include "SubstanceCoreTypedefs.h"
#include "SubstanceInput.h"
#include "framework/imageinput.h"

#include "substance_public.h"

#include <vector>

namespace Substance
{
namespace Details
{

//! @brief Shared pointer on ImageInput (typdef shortcut)
typedef std::shared_ptr<ImageInput> ImageInputPtr;

//! @brief Dynamic array of pointers on ImageInput, used w/ InputState
typedef std::vector<ImageInputPtr> ImageInputPtrs;

//! @brief Input Instance State
//! Contains a input value
struct InputState
{
	//! @brief Input state flags
	enum Flag
	{
		Flag_Constified = 0x80000000u,   //!< This input is constified
		Flag_Cache =      0x40000000u,   //!< This input should be cached
		Flag_CacheOnly =  0x10000000u,   //!< Cache only, value not modified
		
		Flag_MASKTYPE   = 0xFFu,         //!< Type selection mask
		Flag_FORCEDWORD = 0xFFFFFFFFu    //!< Internal Use Only
	};

	//! @brief SubstanceInputType and suppl. flags (combination of Flag)
	uint32 flags;           
	
	union
	{
		//! @brief Numeric values (float/int), for numeric inputs
		uint32 numeric[4];      
		
		//! @brief Index of shared pointer on ImageInput (in external array)
		//! Image input only
		size_t imagePtrIndex; 
	} value;
	
	//! @brief Invalid image pointer index
	static const size_t invalidIndex;
	
	//! @brief Accessor on input type
	SubstanceInputType getType() const { return (SubstanceInputType)(flags&Flag_MASKTYPE); }
	
	//! @brief Return if image type
	bool isImage() const { return Substance_IType_Image==getType(); }
	
	//! @brief Return if is a valid image (not NULL pointer image)
	bool isValidImage() const { return isImage() && value.imagePtrIndex!=invalidIndex; }
	
	//! @brief Accessor on constified state
	bool isConstified() const { return (flags&Flag_Constified)!=0; }
	
	//! @brief Accessor on cache flag
	bool isCacheEnabled() const { return (flags&Flag_Cache)!=0; }
	
	//! @brief Accessor on cache only flag
	bool isCacheOnly() const { return (flags&Flag_CacheOnly)!=0; }
	
	//! @brief Is image pointer index valid
	bool isIndexValid() const { return invalidIndex!=value.imagePtrIndex; }
	
	//! @brief Set input state value from instance
	//! @param[out] dst Destination input state
	//! @param inst Input instance to get value
	//! @param[in,out] imagePtrs Array of pointers on ImageInput indexed by 
	//!		imagePtrIndex
	void fillValue(const FInputInstanceBase* inst, ImageInputPtrs &imagePtrs);
	
	//! @brief Initialize value from description default value
	//! @param desc Input description to get default value
	void initValue(input_desc_t* desc);
	
};  // struct InputState

} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSINPUTSTATE_H
