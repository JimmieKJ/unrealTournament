//! @file SubstanceCache.h
//! @brief Substance Cache Data
//! @date 20140731
//! @copyright Allegorithmic. All rights reserved.
#pragma once

#include "substance_public.h"

class FArchive;
class USubstanceTexture2D;

namespace Substance
{
	class SubstanceCache
	{
	public:
		static TSharedPtr<SubstanceCache> Get()
		{
			if (!SbsCache.IsValid())
			{
				SbsCache = MakeShareable(new SubstanceCache);
			}
			return SbsCache;
		}

		static void Shutdown()
		{
			if (!SbsCache.IsValid())
			{
				SbsCache.Reset();
			}
		}

		bool CanReadFromCache(FGraphInstance* graph);

		bool ReadFromCache(FGraphInstance* graph);

		void CacheOutput(output_inst_t* Output, const SubstanceTexture& result);

	private:
		FString GetPathForGuid(const FGuid& guid) const;
		bool SerializeTexture(FArchive& Ar, SubstanceTexture& result) const;

		static TSharedPtr<SubstanceCache> SbsCache;
	};
}
