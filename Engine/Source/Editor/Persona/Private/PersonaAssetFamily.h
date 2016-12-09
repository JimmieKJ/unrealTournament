// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "AssetData.h"
#include "IAssetFamily.h"

class UAnimBlueprint;

class FPersonaAssetFamily : public IAssetFamily
{
public:
	FPersonaAssetFamily(const UObject* InFromObject);
	virtual ~FPersonaAssetFamily() {}

	/** IAssetFamily interface */
	virtual void GetAssetTypes(TArray<UClass*>& OutAssetTypes) const override;
	virtual FAssetData FindAssetOfType(UClass* InAssetClass) const override;
	virtual void FindAssetsOfType(UClass* InAssetClass, TArray<FAssetData>& OutAssets) const override;
	virtual FText GetAssetTypeDisplayName(UClass* InAssetClass) const override;
	virtual bool IsAssetCompatible(const FAssetData& InAssetData) const override;
	virtual UClass* GetAssetFamilyClass(UClass* InClass) const override;
	virtual void RecordAssetOpened(const FAssetData& InAssetData) override;
	DECLARE_DERIVED_EVENT(FPersonaAssetFamily, IAssetFamily::FOnAssetOpened, FOnAssetOpened)
	virtual FOnAssetOpened& GetOnAssetOpened() override { return OnAssetOpened;  }

private:
	/** The skeleton that links all assets */
	TWeakObjectPtr<const USkeleton> Skeleton;

	/** The last mesh that was encountered */
	TWeakObjectPtr<const USkeletalMesh> Mesh;

	/** The last anim blueprint that was encountered */
	TWeakObjectPtr<const UAnimBlueprint> AnimBlueprint;

	/** The last animation asset that was encountered */
	TWeakObjectPtr<const UAnimationAsset> AnimationAsset;

	/** Event fired when an asset is opened */
	FOnAssetOpened OnAssetOpened;
};
