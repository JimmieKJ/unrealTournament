//! @file detailsoutputstate.h
//! @brief Substance Air Framework Output Instance State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20150609
//! @copyright Allegorithmic. All rights reserved.
//!
 

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSOUTPUTSTATE_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSOUTPUTSTATE_H

#include "SubstanceFOutput.h"


namespace Substance
{
namespace Details
{


//! @brief Output Instance State
//! Contains output format override values
struct OutputState
{
	//! @brief Format override structure, identity if formatOverridden is true
	int32 formatOverride;

	//! @brief Optimization flag: format is really overridden only if true
	bool formatOverridden;
	
	
	//! @brief Default constructor: not overridden
	OutputState() : formatOverridden(false) {}

	//! @brief Set output state value from instance
	//! @param inst Output instance to get format override
	void fill(const FOutputInstance& inst);
	
};  // struct OutputState


} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSOUTPUTSTATE_H
