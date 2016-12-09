// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Editor only class for UI of linking animation curve to joints
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BoneContainer.h"
#include "Animation/SmartName.h"
#include "EditorAnimCurveBoneLinks.generated.h"

class IEditableSkeleton;
struct FPropertyChangedEvent;

DECLARE_DELEGATE_OneParam( FOnAnimCurveBonesChange, class UEditorAnimCurveBoneLinks*)

UCLASS(MinimalAPI)
class UEditorAnimCurveBoneLinks: public UObject
{
	GENERATED_UCLASS_BODY()
public:
	
	virtual void Initialize(const TWeakPtr<class IEditableSkeleton> InEditableSkeleton, const FSmartName& InCurveName, FOnAnimCurveBonesChange OnChangeIn);

	TWeakPtr<class IEditableSkeleton> EditableSkeleton;
	FOnAnimCurveBonesChange OnChange;

	UPROPERTY(VisibleAnywhere, Category = CurveName)
	FSmartName CurveName;

	UPROPERTY(EditAnywhere, Category = Bones)
	TArray<FBoneReference> ConnectedBones;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// refresh current Connected Bones data 
	UNREALED_API void Refresh(const FSmartName& InCurveName, const TArray<FBoneReference>& CurrentLinks);
};
