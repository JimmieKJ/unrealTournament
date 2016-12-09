// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaAssetFamily.h"
#include "Modules/ModuleManager.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "Animation/AnimBlueprint.h"

#define LOCTEXT_NAMESPACE "PersonaAssetFamily"

FPersonaAssetFamily::FPersonaAssetFamily(const UObject* InFromObject)
	: Skeleton(nullptr)
	, Mesh(nullptr)
	, AnimBlueprint(nullptr)
	, AnimationAsset(nullptr)
{
	if (InFromObject)
	{
		if (InFromObject->IsA<USkeleton>())
		{
			Skeleton = CastChecked<USkeleton>(InFromObject);
			Mesh = Skeleton->GetPreviewMesh();
		}
		else if (InFromObject->IsA<UAnimationAsset>())
		{
			AnimationAsset = CastChecked<UAnimationAsset>(InFromObject);
			Skeleton = AnimationAsset->GetSkeleton();
			Mesh = AnimationAsset->GetPreviewMesh();
			if (Mesh == nullptr)
			{
				Mesh = Skeleton->GetPreviewMesh();
			}
		}
		else if (InFromObject->IsA<USkeletalMesh>())
		{
			Mesh = CastChecked<USkeletalMesh>(InFromObject);
			Skeleton = Mesh->Skeleton;
		}
		else if (InFromObject->IsA<UAnimBlueprint>())
		{
			AnimBlueprint = CastChecked<UAnimBlueprint>(InFromObject);
			Skeleton = AnimBlueprint->TargetSkeleton;
			check(AnimBlueprint->TargetSkeleton);
			Mesh = AnimBlueprint->TargetSkeleton->GetPreviewMesh();
		}
	}
}

void FPersonaAssetFamily::GetAssetTypes(TArray<UClass*>& OutAssetTypes) const
{
	OutAssetTypes.Reset();
	OutAssetTypes.Add(USkeleton::StaticClass());
	OutAssetTypes.Add(USkeletalMesh::StaticClass());
	OutAssetTypes.Add(UAnimationAsset::StaticClass());
	OutAssetTypes.Add(UAnimBlueprint::StaticClass());
}

template<typename AssetType>
static void FindAssets(const USkeleton* InSkeleton, TArray<FAssetData>& OutAssetData, FName SkeletonTag)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassNames.Add(AssetType::StaticClass()->GetFName());
	Filter.TagsAndValues.Add(SkeletonTag, FAssetData(InSkeleton).GetExportTextName());

	AssetRegistryModule.Get().GetAssets(Filter, OutAssetData);
}

FAssetData FPersonaAssetFamily::FindAssetOfType(UClass* InAssetClass) const
{
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			// we should always have a skeleton here, this asset family is based on it
			return FAssetData(Skeleton.Get());
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			if (AnimationAsset.IsValid())
			{
				return FAssetData(AnimationAsset.Get());
			}
			else
			{
				TArray<FAssetData> Assets;
				FindAssets<UAnimationAsset>(Skeleton.Get(), Assets, "Skeleton");
				if (Assets.Num() > 0)
				{
					return Assets[0];
				}
			}
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			if (Mesh.IsValid())
			{
				return FAssetData(Mesh.Get());
			}
			else
			{
				TArray<FAssetData> Assets;
				FindAssets<USkeletalMesh>(Skeleton.Get(), Assets, "Skeleton");
				if (Assets.Num() > 0)
				{
					return Assets[0];
				}
			}
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			if (AnimBlueprint.IsValid())
			{
				return FAssetData(AnimBlueprint.Get());
			}
			else
			{
				TArray<FAssetData> Assets;
				FindAssets<UAnimBlueprint>(Skeleton.Get(), Assets, "TargetSkeleton");
				if (Assets.Num() > 0)
				{
					return Assets[0];
				}
			}
		}
	}

	return FAssetData();
}

void FPersonaAssetFamily::FindAssetsOfType(UClass* InAssetClass, TArray<FAssetData>& OutAssets) const
{
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			// we should always have a skeleton here, this asset family is based on it
			OutAssets.Add(FAssetData(Skeleton.Get()));
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			FindAssets<UAnimationAsset>(Skeleton.Get(), OutAssets, "Skeleton");
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			FindAssets<USkeletalMesh>(Skeleton.Get(), OutAssets, "Skeleton");
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			FindAssets<UAnimBlueprint>(Skeleton.Get(), OutAssets, "TargetSkeleton");
		}
	}
}

FText FPersonaAssetFamily::GetAssetTypeDisplayName(UClass* InAssetClass) const
{
	if (InAssetClass)
	{
		if (InAssetClass->IsChildOf<USkeleton>())
		{
			return LOCTEXT("SkeletonAssetDisplayName", "Skeleton");
		}
		else if (InAssetClass->IsChildOf<UAnimationAsset>())
		{
			return LOCTEXT("AnimationAssetDisplayName", "Animation");
		}
		else if (InAssetClass->IsChildOf<USkeletalMesh>())
		{
			return LOCTEXT("SkeletalMeshAssetDisplayName", "Mesh");
		}
		else if (InAssetClass->IsChildOf<UAnimBlueprint>())
		{
			return LOCTEXT("AnimBlueprintAssetDisplayName", "Blueprint");
		}
	}

	return FText();
}

bool FPersonaAssetFamily::IsAssetCompatible(const FAssetData& InAssetData) const
{
	UClass* Class = InAssetData.GetClass();
	if (Class)
	{
		if (Class->IsChildOf<USkeleton>())
		{
			return FAssetData(Skeleton.Get()) == InAssetData;
		}
		else if (Class->IsChildOf<UAnimationAsset>() || Class->IsChildOf<USkeletalMesh>())
		{
			const FString* TargetSkeleton = InAssetData.TagsAndValues.Find("Skeleton");
			if (TargetSkeleton)
			{
				return *TargetSkeleton == FAssetData(Skeleton.Get()).GetExportTextName();
			}
		}
		else if (Class->IsChildOf<UAnimBlueprint>())
		{
			const FString* TargetSkeleton = InAssetData.TagsAndValues.Find("TargetSkeleton");
			if (TargetSkeleton)
			{
				return *TargetSkeleton == FAssetData(Skeleton.Get()).GetExportTextName();
			}
		}
	}

	return false;
}

UClass* FPersonaAssetFamily::GetAssetFamilyClass(UClass* InClass) const
{
	if (InClass)
	{
		if (InClass->IsChildOf<USkeleton>())
		{
			return USkeleton::StaticClass();
		}
		else if (InClass->IsChildOf<UAnimationAsset>())
		{
			return UAnimationAsset::StaticClass();
		}
		else if (InClass->IsChildOf<USkeletalMesh>())
		{
			return USkeletalMesh::StaticClass();
		}
		else if (InClass->IsChildOf<UAnimBlueprint>())
		{
			return UAnimBlueprint::StaticClass();
		}
	}

	return nullptr;
}

void FPersonaAssetFamily::RecordAssetOpened(const FAssetData& InAssetData)
{
	if (IsAssetCompatible(InAssetData))
	{
		UClass* Class = InAssetData.GetClass();
		if (Class)
		{
			if (Class->IsChildOf<USkeleton>())
			{
				Skeleton = Cast<USkeleton>(InAssetData.GetAsset());
			}
			else if (Class->IsChildOf<UAnimationAsset>())
			{
				AnimationAsset = Cast<UAnimationAsset>(InAssetData.GetAsset());
			}
			else if (Class->IsChildOf<USkeletalMesh>())
			{
				Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
			}
			else if (Class->IsChildOf<UAnimBlueprint>())
			{
				AnimBlueprint = Cast<UAnimBlueprint>(InAssetData.GetAsset());
			}
		}

		OnAssetOpened.Broadcast(InAssetData.GetAsset());
	}
}

#undef LOCTEXT_NAMESPACE
 
