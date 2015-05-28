// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"


UMeshComponent::UMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow = true;
	bUseAsOccluder = true;
	bCanEverAffectNavigation = true;
}

void UMeshComponent::BeginDestroy()
{
	// Notify the streaming system.
	IStreamingManager::Get().NotifyPrimitiveDetached( this );

	Super::BeginDestroy();
}

UMaterialInterface* UMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (OverrideMaterials.IsValidIndex(ElementIndex))
	{
		return OverrideMaterials[ElementIndex];
	}
	else
	{
		return nullptr;
	}
}

void UMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	if (ElementIndex >= 0)
	{
		if (OverrideMaterials.IsValidIndex(ElementIndex) && (OverrideMaterials[ElementIndex] == Material))
		{
			// Do nothing, the material is already set
		}
		else
		{
			// Grow the array if the new index is too large
			if (OverrideMaterials.Num() <= ElementIndex)
			{
				OverrideMaterials.AddZeroed(ElementIndex + 1 - OverrideMaterials.Num());
			}

			// Set the material and invalidate things
			OverrideMaterials[ElementIndex] = Material;
			MarkRenderStateDirty();

			FBodyInstance* BodyInst = GetBodyInstance();
			if (BodyInst && BodyInst->IsValidBodyInstance())
			{
				BodyInst->UpdatePhysicalMaterials();
			}
		}
	}
}

FMaterialRelevance UMeshComponent::GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Combine the material relevance for all materials.
	FMaterialRelevance Result;
	for(int32 ElementIndex = 0;ElementIndex < GetNumMaterials();ElementIndex++)
	{
		UMaterialInterface const* MaterialInterface = GetMaterial(ElementIndex);
		if(!MaterialInterface)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		Result |= MaterialInterface->GetRelevance_Concurrent(InFeatureLevel);
	}
	return Result;
}

int32 UMeshComponent::GetNumOverrideMaterials() const
{
	return OverrideMaterials.Num();
}

int32 UMeshComponent::GetNumMaterials() const
{
	return 0;
}

void UMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	for (int32 ElementIndex = 0; ElementIndex < GetNumMaterials(); ElementIndex++)
	{
		if (UMaterialInterface* MaterialInterface = GetMaterial(ElementIndex))
		{
			OutMaterials.Add(MaterialInterface);
		}
	}
}

void UMeshComponent::PrestreamTextures( float Seconds, bool bPrioritizeCharacterTextures, int32 CinematicTextureGroups )
{
	// If requested, tell the streaming system to only process character textures for 30 frames.
	if (bPrioritizeCharacterTextures)
	{
		IStreamingManager::Get().SetDisregardWorldResourcesForFrames(30);
	}

	TArray<UTexture*> Textures;
	GetUsedTextures(/*out*/ Textures, EMaterialQualityLevel::Num);

	for (UTexture* Texture : Textures)
	{
		if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
		{
			Texture2D->SetForceMipLevelsToBeResident(Seconds, CinematicTextureGroups);
		}
	}
}

void UMeshComponent::SetTextureForceResidentFlag( bool bForceMiplevelsToBeResident )
{
	const int32 CinematicTextureGroups = 0;
	const float Seconds = -1.0f;

	TArray<UTexture*> Textures;
	GetUsedTextures(/*out*/ Textures, EMaterialQualityLevel::Num);

	for (UTexture* Texture : Textures)
	{
		if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
		{
			Texture2D->SetForceMipLevelsToBeResident(Seconds, CinematicTextureGroups);
			Texture2D->bForceMiplevelsToBeResident = bForceMiplevelsToBeResident;
		}
	}
}


TArray<class UMaterialInterface*> UMeshComponent::GetMaterials() const
{
	TArray<class UMaterialInterface*> OutMaterials;
	int32 TotalNumMaterials = GetNumMaterials();
	if(TotalNumMaterials > 0)
	{
		// make sure to extend it
		OutMaterials.AddZeroed(TotalNumMaterials);

		for(int32 MaterialIndex=0; MaterialIndex < TotalNumMaterials; ++MaterialIndex)
		{
			OutMaterials[MaterialIndex] = GetMaterial(MaterialIndex);
		}
	}

	return OutMaterials;
}
