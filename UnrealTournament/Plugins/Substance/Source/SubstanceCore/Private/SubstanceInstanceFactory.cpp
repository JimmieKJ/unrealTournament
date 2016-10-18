//! @file SubstanceInstanceFactory.cpp
//! @brief Implementation of the USubstanceInstanceFactory class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceInstanceFactory.h"
#include "SubstanceFGraph.h"
#include "SubstanceTexture2D.h"
#include "SubstanceSettings.h"
#include "framework/details/detailslinkdata.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "ContentBrowserModule.h"
#endif

USubstanceInstanceFactory::USubstanceInstanceFactory(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	GenerationMode = GetDefault<USubstanceSettings>()->DefaultGenerationMode;
}


ESubstanceGenerationMode USubstanceInstanceFactory::GetGenerationMode() const
{
    if (GenerationMode == ESubstanceGenerationMode::SGM_PlatformDefault)
    {
		ESubstanceGenerationMode systemMode = GetDefault<USubstanceSettings>()->DefaultGenerationMode;

		if (systemMode == ESubstanceGenerationMode::SGM_PlatformDefault)
		{
			return SGM_OnLoadAsyncAndCache;
		}
		else
		{
			return systemMode;
		}
    }
    else
    {
        return GenerationMode;
    }
}


bool USubstanceInstanceFactory::ShouldCacheOutput() const
{
#if PLATFORM_PS4
	return false;
#else
	switch (GetGenerationMode())
	{
	case ESubstanceGenerationMode::SGM_OnLoadAsyncAndCache:
	case ESubstanceGenerationMode::SGM_OnLoadSyncAndCache:
		return true;
	default:
		return false;
	}
#endif
}

void USubstanceInstanceFactory::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FSubstanceCoreCustomVersion::GUID);

	Ar << GenerationMode;

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (Ar.IsLoading())
	{
		if (!SubstancePackage)
		{
			SubstancePackage = new Substance::FPackage();
			SubstancePackage->Parent = this;
		}
	}

	if (SubstancePackage)
	{
		if (bCooked)
		{
			if (Ar.IsSaving())
			{
				// check if we have baked or dynamic textures
				// then keep or drop the sbsar file accordingly

				if (GetGenerationMode() == ESubstanceGenerationMode::SGM_Baked)
				{
					Substance::Details::LinkDataAssembly *linkdata = 
						static_cast<Substance::Details::LinkDataAssembly*>(
						SubstancePackage->getLinkData().get());

					linkdata->zeroAssembly();
				}
			}
		}
	
		Ar << SubstancePackage;
	}
}


void USubstanceInstanceFactory::BeginDestroy()
{
	Super::BeginDestroy();

	if (SubstancePackage)
	{
		delete SubstancePackage;
		SubstancePackage = 0;
	}
}


void USubstanceInstanceFactory::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	
	if (!bDuplicateForPIE)
	{
		check(SubstancePackage);

		// create a new Guid
		SubstancePackage->Guid = FGuid::NewGuid();

		// re-create the instances from the original
		Substance::List<graph_desc_t*>::TIterator ItDesc(
			SubstancePackage->Graphs.itfront());

#if WITH_EDITOR
		TArray<UObject*> AssetList;
#endif

		for (; ItDesc; ++ItDesc)
		{
			Substance::List<substanceGuid_t> RefInstances = (*ItDesc)->InstanceUids;
			(*ItDesc)->InstanceUids.Empty();

			Substance::List<substanceGuid_t>::TIterator ItGuid(RefInstances.itfront());

			for (; ItGuid; ++ItGuid)
			{
				for (TObjectIterator<USubstanceGraphInstance> ItPrevInst; ItPrevInst; ++ItPrevInst)
				{
					if ((*ItPrevInst)->Instance->InstanceGuid == *ItGuid)
					{
						USubstanceGraphInstance* SourceInstance = *ItPrevInst;

						FString BasePath;
						FString ParentName = this->GetOuter()->GetPathName();

						ParentName.Split(TEXT("/"), &(BasePath), NULL, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

						FString AssetNameStr;
						FString PackageNameStr;

#if WITH_EDITOR
						FString Name = ObjectTools::SanitizeObjectName(SourceInstance->Instance->Desc->Label);
						static FName AssetToolsModuleName = FName("AssetTools");
						FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
						AssetToolsModule.Get().CreateUniqueAssetName(BasePath + TEXT("/") + Name + TEXT("_INST"), TEXT(""), PackageNameStr, AssetNameStr);
#else
						PackageNameStr = BasePath;
#endif
						UObject* InstanceOuter = CreatePackage(NULL, *PackageNameStr);

						graph_inst_t* NewInstance = Substance::Helpers::InstantiateGraph(
							(*ItDesc), InstanceOuter, AssetNameStr, false /*bCreateOutputs*/);

						// copy values from previous
						Substance::Helpers::CopyInstance((*ItPrevInst)->Instance, NewInstance);
						Substance::Helpers::RenderAsync(NewInstance);

#if WITH_EDITOR
						for (auto itout = NewInstance->Outputs.itfront(); itout; ++itout)
						{
							AssetList.AddUnique(*(itout->Texture));
						}
						AssetList.AddUnique(NewInstance->ParentInstance);
#endif
						break;
					}
				}
			}
		}

#if WITH_EDITOR
		if (GIsEditor)
		{
			AssetList.AddUnique(this);
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(AssetList);
		}
#endif
	}
}


SIZE_T USubstanceInstanceFactory::GetResourceSize(EResourceSizeMode::Type Mode)
{
	if (SubstancePackage)
	{
		Substance::Details::LinkDataAssembly *linkdata =
			static_cast<Substance::Details::LinkDataAssembly*>(
			SubstancePackage->getLinkData().get());

		return linkdata->getSize();
	}
	
	return 0;
}
