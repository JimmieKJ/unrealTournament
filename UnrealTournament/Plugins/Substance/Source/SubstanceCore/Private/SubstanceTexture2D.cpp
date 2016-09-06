//! @file SubstanceTexture2D.cpp
//! @brief Implementation of the USubstanceTexture2D class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceInstanceFactory.h"
#include "SubstanceGraphInstance.h"
#include "SubstanceFGraph.h"
#include "SubstanceFOutput.h"
#include "SubstanceTexture2D.h"
#include "SubstanceSettings.h"
#include "SubstanceTexture2DDynamicResource.h"

#if WITH_EDITOR
#include "ObjectTools.h"
#include "ContentBrowserModule.h"
#include "TargetPlatform.h"
#include "ModuleManager.h"
#endif // WITH_EDITOR

USubstanceTexture2D::USubstanceTexture2D(class FObjectInitializer const & PCIP) : Super(PCIP)
{

}


FString USubstanceTexture2D::GetDesc()
{
	return FString::Printf( TEXT("%dx%d[%s]"), SizeX, SizeY, GPixelFormats[Format].Name);
}


void USubstanceTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FSubstanceCoreCustomVersion::GUID);

	Ar << Format;
	Ar << SizeX;
	Ar << SizeY;
	Ar << NumMips;
	Ar << OutputGuid;
	Ar << ParentInstance;

	int32 FirstMipToSerialize = 0;

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (bCooked && !this->IsDefaultSubobject())
	{
		ESubstanceGenerationMode ParentPackageGenerationMode = ESubstanceGenerationMode::SGM_PlatformDefault;

		if (ParentInstance && ParentInstance->Parent)
		{
			ParentPackageGenerationMode = ParentInstance->Parent->GetGenerationMode();
			if (ParentPackageGenerationMode == ESubstanceGenerationMode::SGM_PlatformDefault)
			{
				ParentPackageGenerationMode = GetDefault<USubstanceSettings>()->DefaultGenerationMode;
			}
		}

		if (Ar.IsSaving())
		{
			int32 MipsToRegen = 0; // number of mipmap levels which will not be saved ie regenerated during loading

			switch (ParentPackageGenerationMode)
			{
			case ESubstanceGenerationMode::SGM_Baked:
				MipsToRegen = 0;
				break;
			case ESubstanceGenerationMode::SGM_OnLoadSync:
			case ESubstanceGenerationMode::SGM_OnLoadSyncAndCache:
				MipsToRegen = NumMips - 1; //keep the bottom of the mipmap pyramid
				break;
			case ESubstanceGenerationMode::SGM_OnLoadAsync:
			case ESubstanceGenerationMode::SGM_OnLoadAsyncAndCache:
				MipsToRegen = FMath::Clamp(GetDefault<USubstanceSettings>()->AsyncLoadMipClip, 1, NumMips);
				break;
			default:
				MipsToRegen = NumMips - 1;
				break;
			}

			FirstMipToSerialize += FMath::Max(0, MipsToRegen);
		}

		Ar << FirstMipToSerialize;
	}
	
	if (Ar.IsSaving())
	{
		for (int32 MipIndex = 0; MipIndex < NumMips - FirstMipToSerialize; ++MipIndex)
		{
			Mips[MipIndex + FirstMipToSerialize].Serialize(Ar, this, MipIndex);
		}
	}
	else if (Ar.IsLoading())
	{
		NumMips -= FirstMipToSerialize;
		Mips.Empty(NumMips);
		
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			new(Mips) FTexture2DMipMap();
			Mips[MipIndex].Serialize(Ar, this, MipIndex);
		}
	}
}


void USubstanceTexture2D::BeginDestroy()
{
	// Route BeginDestroy.
	Super::BeginDestroy();

	if (OutputCopy)
	{
		// nullify the pointer to this texture's address
		// so that others see it has been destroyed
		*OutputCopy->Texture.get() = NULL;

		delete OutputCopy;
		OutputCopy = 0;

		// disable the output in the parent instance
		if (ParentInstance && ParentInstance->Instance)
		{
			Substance::Helpers::ClearFromRender(ParentInstance);

			Substance::List<output_inst_t>::TIterator 
				ItOut(ParentInstance->Instance->Outputs.itfront());

			for(; ItOut ; ++ItOut)
			{
				if ((*ItOut).OutputGuid == OutputGuid)
				{
					(*ItOut).bIsEnabled = false;
					break;
				}
			}
		}
	}
}


#if WITH_EDITOR
bool USubstanceTexture2D::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);

	if (bIsEditable && InProperty != NULL)
	{
		bIsEditable = false;

		if (InProperty->GetFName() == TEXT("AddressX") ||
			InProperty->GetFName() == TEXT("AddressY") ||
			InProperty->GetFName() == TEXT("UnpackMin") ||
			InProperty->GetFName() == TEXT("UnpackMax") ||
			InProperty->GetFName() == TEXT("Filter") ||
			InProperty->GetFName() == TEXT("LODBias") ||
			InProperty->GetFName() == TEXT("sRGB") ||
			InProperty->GetFName() == TEXT("LODGroup"))
		{
			bIsEditable = true;
		}
	}

	return bIsEditable;
}
#endif


void USubstanceTexture2D::PostLoad()
{
	if (NULL == ParentInstance)
	{
		UE_LOG(LogSubstanceCore, Error, TEXT("No parent instance found for this SubstanceTexture2D (%s). You need to delete the texture."),*GetFullName());
		Super::PostLoad();
		return;
	}

	ParentInstance->ConditionalPostLoad(); // make sure the parent instance is loaded
	
	// find the output using the UID serialized
	output_inst_t* Output = NULL;
	Substance::List<output_inst_t>::TIterator 
		OutIt(ParentInstance->Instance->Outputs.itfront());

	for ( ; OutIt ; ++OutIt)
	{
		if ((*OutIt).OutputGuid == OutputGuid)
		{
			Output = &(*OutIt);
			break;
		}
	}

	if (!Output)
	{
		// the opposite situation is possible, no texture but an OutputInstance,
		// in this case the OutputInstance is disabled, but when the texture is
		// alive the Output must exist.
		UE_LOG(LogSubstanceCore, Error, TEXT("No matching output found for this SubstanceTexture2D (%s). You need to delete the texture and its parent instance."),*GetFullName());
		Super::PostLoad();
		return;
	}
	else
	{
		// integrity check, used to detect instance / desc mismatch
		if (!Output->GetOutputDesc())
		{
			UE_LOG(LogSubstanceCore, Error, TEXT("No matching output description found for this SubstanceTexture2D (%s). You need to delete the texture and its parent instance."),*GetFullName());
			Super::PostLoad();
			return;
		}

		// link the corresponding output to this texture 
		// this is already done when duplicate textures
		if (*Output->Texture == NULL)
		{
			// build the copy of the Output Instance
			*Output->Texture = this;
			OutputCopy = new output_inst_t(*Output);
		}

		// enable this output
		Output->bIsEnabled = true;
		
		if (ParentInstance->Parent->GetGenerationMode() != ESubstanceGenerationMode::SGM_Baked)
		{
			Output->bIsDirty = true;
		}
	}

	Super::PostLoad();
}


void USubstanceTexture2D::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		USubstanceTexture2D* RefTexture = NULL;

		for (TObjectIterator<USubstanceTexture2D> It; It; ++It)
		{
			if ((*It)->OutputGuid == OutputGuid && *It != this)
			{
				RefTexture = *It;
				break;
			}
		}
		check(RefTexture);

		// after duplication, we need to recreate a parent instance
		// look for the original object, using the GUID
		USubstanceGraphInstance* RefInstance = RefTexture->ParentInstance;

		if (!RefInstance || !RefInstance->Instance)
		{
			return;
		}

		USubstanceGraphInstance* NewGraphInstance = Substance::Helpers::DuplicateGraphInstance(RefInstance);

		// now we need to bind this to the new instance 
		// and bind its output to this
		ParentInstance = NewGraphInstance;

		Substance::List<output_inst_t>::TIterator ItOut(NewGraphInstance->Instance->Outputs.itfront());

		for (; ItOut; ++ItOut)
		{
			if ((*ItOut).Uid == RefTexture->OutputCopy->Uid)
			{
				(*ItOut).bIsEnabled = true;
				this->OutputGuid = (*ItOut).OutputGuid;
				*(*ItOut).Texture.get() = this;
				this->OutputCopy =
					new Substance::FOutputInstance(*ItOut);
				break;
			}
		}

		Substance::Helpers::RenderAsync(NewGraphInstance->Instance);

#if WITH_EDITOR
		if (GIsEditor)
		{
			TArray<UObject*> AssetList;
			for (auto itout = NewGraphInstance->Instance->Outputs.itfront(); itout; ++itout)
			{
				AssetList.AddUnique(*(itout->Texture));
			}
			AssetList.AddUnique(NewGraphInstance);

			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(AssetList);
		}
#endif
	}
}


SIZE_T USubstanceTexture2D::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T sum = 0;
	for (auto it = Mips.CreateConstIterator(); it; ++it)
	{
		sum += it->BulkData.GetBulkDataSize();
	}
	return sum;
}


FTextureResource* USubstanceTexture2D::CreateResource()
{
	if (Mips.Num())
	{
		return new FSubstanceTexture2DDynamicResource(this);
	}

	return NULL;
}


void USubstanceTexture2D::UpdateResource()
{
	Super::UpdateResource();

	if (Resource)
	{
		struct FUpdateSubstanceTexture
		{
			FTexture2DDynamicResource* Resource;
			USubstanceTexture2D* Owner;
			TArray<const void*>	MipData;
		};

		FUpdateSubstanceTexture* SubstanceData = new FUpdateSubstanceTexture;

		SubstanceData->Resource = (FTexture2DDynamicResource*)Resource;
		SubstanceData->Owner = this;

		for (int32 MipIndex = 0; MipIndex < SubstanceData->Owner->Mips.Num(); MipIndex++)
		{
			FTexture2DMipMap& MipMap = SubstanceData->Owner->Mips[MipIndex];

			//this gets unlocked in render command
			const void* MipData = MipMap.BulkData.LockReadOnly();
			SubstanceData->MipData.Add(MipData);
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateSubstanceTexture,
			FUpdateSubstanceTexture*,SubstanceData,SubstanceData,
		{
			// Read the resident mip-levels into the RHI texture.
			for( int32 MipIndex=0; MipIndex<SubstanceData->Owner->Mips.Num(); MipIndex++ )
			{
				uint32 DestPitch;
				void* TheMipData = RHILockTexture2D( SubstanceData->Resource->GetTexture2DRHI(), MipIndex, RLM_WriteOnly, DestPitch, false );

				FTexture2DMipMap& MipMap = SubstanceData->Owner->Mips[MipIndex];
				const void* MipData = SubstanceData->MipData[MipIndex];

				// for platforms that returned 0 pitch from Lock, we need to just use the bulk data directly, never do 
				// runtime block size checking, conversion, or the like
				if (DestPitch == 0)
				{
					FMemory::Memcpy(TheMipData, MipData, MipMap.BulkData.GetBulkDataSize());
				}
				else
				{
					EPixelFormat PixelFormat = SubstanceData->Owner->Format;
					const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;		// Block width in pixels
					const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;		// Block height in pixels
					const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
					uint32 NumColumns		= (MipMap.SizeX + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
					uint32 NumRows			= (MipMap.SizeY + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
					if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
					{
						// PVRTC has minimum 2 blocks width and height
						NumColumns = FMath::Max<uint32>(NumColumns, 2);
						NumRows = FMath::Max<uint32>(NumRows, 2);
					}
					const uint32 SrcPitch   = NumColumns * BlockBytes;						// Num-of bytes per row in the source data
					const uint32 EffectiveSize = BlockBytes*NumColumns*NumRows;

					// Copy the texture data.
					CopyTextureData2D(MipData,TheMipData,MipMap.SizeY,PixelFormat,SrcPitch,DestPitch);
				}

				MipMap.BulkData.Unlock();

				RHIUnlockTexture2D( SubstanceData->Resource->GetTexture2DRHI(), MipIndex, false );
			}

			delete SubstanceData;
		});
	}
}


/** Create RHI sampler states. */
void FSubstanceTexture2DDynamicResource::CreateSamplerStates(float MipMapBias)
{
	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
		(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(SubstanceOwner),
		SubstanceOwner->AddressX == TA_Wrap ? AM_Wrap : (SubstanceOwner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		SubstanceOwner->AddressY == TA_Wrap ? AM_Wrap : (SubstanceOwner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap,
		MipMapBias
		);
	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

	// Create a custom sampler state for using this texture in a deferred pass, where ddx / ddy are discontinuous
	FSamplerStateInitializerRHI DeferredPassSamplerStateInitializer
		(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(SubstanceOwner),
		SubstanceOwner->AddressX == TA_Wrap ? AM_Wrap : (SubstanceOwner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		SubstanceOwner->AddressY == TA_Wrap ? AM_Wrap : (SubstanceOwner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap,
		MipMapBias,
		// Disable anisotropic filtering, since aniso doesn't respect MaxLOD
		1,
		0,
		// Prevent the less detailed mip levels from being used, which hides artifacts on silhouettes due to ddx / ddy being very large
		// This has the side effect that it increases minification aliasing on light functions
		2
		);

	DeferredPassSamplerStateRHI = RHICreateSamplerState(DeferredPassSamplerStateInitializer);
}


/** Called when the resource is initialized. This is only called by the rendering thread. */
void FSubstanceTexture2DDynamicResource::InitRHI()
{
	float MipMapBias =
		FMath::Clamp<float>(
			UTexture2D::GetGlobalMipMapLODBias() + ((SubstanceOwner->LODGroup == TEXTUREGROUP_UI) ?
					- SubstanceOwner->Mips.Num() :
					SubstanceOwner->LODBias), 
			0,
			SubstanceOwner->Mips.Num());

	// Create the sampler state RHI resource.
	CreateSamplerStates(MipMapBias);

	uint32 Flags = 0;
	if (SubstanceOwner->bIsResolveTarget)
	{
		Flags |= TexCreate_ResolveTargetable;
		bIgnoreGammaConversions = true;		// Note, we're ignoring Owner->SRGB (it should be false).
	}

	if (SubstanceOwner->SRGB)
	{
		Flags |= TexCreate_SRGB;
	}
	else
	{
		bIgnoreGammaConversions = true;
		bSRGB = false;
	}

	if (SubstanceOwner->bNoTiling)
	{
		Flags |= TexCreate_NoTiling;
	}
	FRHIResourceCreateInfo CreateInfo;
	Texture2DRHI = RHICreateTexture2D(GetSizeX(), GetSizeY(), SubstanceOwner->Format, SubstanceOwner->NumMips, 1, Flags, CreateInfo);
	TextureRHI = Texture2DRHI;
	RHIUpdateTextureReference(SubstanceOwner->TextureReference.TextureReferenceRHI, TextureRHI);
}


/** Called when the resource is released. This is only called by the rendering thread. */
void FSubstanceTexture2DDynamicResource::ReleaseRHI()
{
	RHIUpdateTextureReference(SubstanceOwner->TextureReference.TextureReferenceRHI, FTextureRHIParamRef());
	FTextureResource::ReleaseRHI();
	Texture2DRHI.SafeRelease();
}


/** Returns the Texture2DRHI, which can be used for locking/unlocking the mips. */
FTexture2DRHIRef FSubstanceTexture2DDynamicResource::GetTexture2DRHI()
{
	return Texture2DRHI;
}

