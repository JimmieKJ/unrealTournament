// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once
#include "PoseAssetFactory.generated.h"

UCLASS(HideCategories=Object,MinimalAPI)
class UPoseAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class USkeleton* TargetSkeleton;

	/* Used when creating a composite from an AnimSequence, becomes the only AnimSequence contained */
	UPROPERTY()
	class UAnimSequence* SourceAnimation;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

private:
	void OnTargetSkeletonSelected(const FAssetData& SelectedAsset);

private:
	TSharedPtr<SWindow> PickerWindow;
};

