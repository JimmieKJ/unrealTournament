//! @file renderopt.h
//! @brief Substance definition of render options used at renderer creation
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_FRAMEWORK_RENDEROPT_H
#define _SUBSTANCE_FRAMEWORK_RENDEROPT_H

#include "SubstanceCoreTypedefs.h"
#include "SubstanceSettings.h"

namespace Substance
{

//! @brief Substance output options, used at renderer creation
//! Allows to set initial memory consumption budget.
struct RenderOptions
{
	//! @brief Initial memory budget allowed to renderer, in bytes.
	//! The default value is set to 512MB.
	size_t mMemoryBudget;

	size_t mCoresCount;

	//! @brief Default constructor
	RenderOptions()
	{
		size_t BudgetMb = FMath::Clamp(GetDefault<USubstanceSettings>()->MemoryBudgetMb, SBS_MIN_MEM_BUDGET, SBS_MAX_MEM_BUDGET);
		int32 CPUCores = FMath::Clamp(GetDefault<USubstanceSettings>()->CPUCores, (int32)1, FPlatformMisc::NumberOfCores());

		mMemoryBudget = BudgetMb * 1024 * 1024;
		mCoresCount = CPUCores;
	}
};

} // namespace Substance

#endif //_SUBSTANCE_FRAMEWORK_RENDEROPT_H
