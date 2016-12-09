// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorAnimCurveBones.cpp: Montage classes that contains slots
=============================================================================*/ 

#include "Animation/EditorAnimCurveBoneLinks.h"

#define LOCTEXT_NAMESPACE "UEditorAnimCurveBoneLinks"

UEditorAnimCurveBoneLinks::UEditorAnimCurveBoneLinks(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEditorAnimCurveBoneLinks::Initialize(const TWeakPtr<class IEditableSkeleton> InEditableSkeleton, const FSmartName& InCurveName, FOnAnimCurveBonesChange OnChangeIn)
{
	EditableSkeleton = InEditableSkeleton;
	CurveName = InCurveName;
	OnChange = OnChangeIn;
	SetFlags(RF_Transactional);
}

void UEditorAnimCurveBoneLinks::Refresh(const FSmartName& InCurveName, const TArray<FBoneReference>& CurrentLinks)
{
	if (EditableSkeleton.IsValid())
	{
		// double check the name
		CurveName = InCurveName;
		ConnectedBones = CurrentLinks;
	}
}

void UEditorAnimCurveBoneLinks::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(OnChange.IsBound())
	{
		OnChange.Execute(this);
	}
}

#undef LOCTEXT_NAMESPACE
