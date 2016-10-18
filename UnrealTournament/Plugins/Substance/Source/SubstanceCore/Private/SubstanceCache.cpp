//! @file SubstanceCache.cpp
//! @brief Substance Cache Data
//! @date 20140731
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreClasses.h"
#include "SubstanceCache.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceTexture2D.h"
#include "SubstanceFGraph.h"
#include "SubstanceInstanceFactory.h"

#include "Paths.h"

#define SUBSTANCECACHE_VERSION 1

using namespace Substance;

TSharedPtr<SubstanceCache> SubstanceCache::SbsCache;

bool SubstanceCache::CanReadFromCache(FGraphInstance* graph)
{
#if PLATFORM_PS4
	// disable caching for PS4
	return false;
#else
	bool canReadFromCache = false;

	//make sure all enabled outputs have a cached file
	{
		auto iter = graph->Outputs.itfront();
		while (iter)
		{
			if ((*iter).bIsEnabled)
			{
				FString path = GetPathForGuid((*iter).OutputGuid);

				if (IFileManager::Get().FileSize(*path) == INDEX_NONE)
				{
					return false;
				}
				else
				{
					canReadFromCache = true;
				}
			}

			iter++;
		}
	}

	return canReadFromCache;
#endif
}

bool SubstanceCache::ReadFromCache(FGraphInstance* graph)
{
#if PLATFORM_PS4
	// disable caching for PS4
	return false;
#else
	if (!graph->ParentInstance->Parent->ShouldCacheOutput())
	{
		return false;
	}

	if (!CanReadFromCache(graph))
	{
		return false;
	}
	
	//read in cached data and upload
	auto iter = graph->Outputs.itfront();
	while (iter)
	{
		if ((*iter).bIsEnabled)
		{
			FOutputInstance& outputInstance = *(iter);
			FString path = GetPathForGuid(outputInstance.OutputGuid);
			FArchive* Ar = IFileManager::Get().CreateFileReader(*path);

			if (Ar)
			{
				SubstanceTexture output;
				FMemory::Memzero(output);

				if (SerializeTexture(*Ar, output))
				{
					delete Ar;

					USubstanceTexture2D* texture = *((outputInstance.Texture).get());
					Substance::Helpers::UpdateTexture(output, &outputInstance, false);

#if SUBSTANCE_MEMORY_STAT
					DEC_MEMORY_STAT_BY(STAT_SubstanceCacheMemory, FMemory::GetAllocSize(output.buffer));
#endif
					FMemory::Free(output.buffer);					
				}
				else
				{
					delete Ar;
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		iter++;
	}

	return true;
#endif
}

void SubstanceCache::CacheOutput(output_inst_t* output, const SubstanceTexture& result)
{
#if !PLATFORM_PS4
	FString filename = GetPathForGuid(output->OutputGuid);
	FArchive* Ar = IFileManager::Get().CreateFileWriter(*filename, 0);

	if (Ar)
	{
		SerializeTexture(*Ar, const_cast<SubstanceTexture&>(result));
		delete Ar;
	}
#endif
}

FString SubstanceCache::GetPathForGuid(const FGuid& guid) const
{
	return FString::Printf(TEXT("%s/Substance/%s.cache"), *FPaths::GameSavedDir(), *(guid.ToString()));
}

bool SubstanceCache::SerializeTexture(FArchive& Ar, SubstanceTexture& result) const
{
	uint32 version = SUBSTANCECACHE_VERSION;

	Ar << version;

	if (Ar.IsLoading())
	{
		if (version != SUBSTANCECACHE_VERSION)
		{
			UE_LOG(LogSubstanceCore, Warning, TEXT("Out of date Substance cache entry, will regenerate"));
			return false;
		}
	}

	Ar << result.level0Width;
	Ar << result.level0Height;
	Ar << result.pixelFormat;
	Ar << result.channelsOrder;
	Ar << result.mipmapCount;

	EPixelFormat pixelFormat = Substance::Helpers::SubstanceToUe4Format((SubstancePixelFormat)result.pixelFormat, (SubstanceChannelsOrder)result.channelsOrder);
	SIZE_T bufferSize = CalcTextureSize(result.level0Width, result.level0Height, pixelFormat, result.mipmapCount);

	if (Ar.IsLoading())
	{
		check(result.buffer == NULL);
		result.buffer = FMemory::Malloc(bufferSize);
#if SUBSTANCE_MEMORY_STAT
		INC_MEMORY_STAT_BY(STAT_SubstanceCacheMemory, bufferSize);
#endif
	}
	
	Ar.Serialize(result.buffer, bufferSize);
	
	return true;
}
