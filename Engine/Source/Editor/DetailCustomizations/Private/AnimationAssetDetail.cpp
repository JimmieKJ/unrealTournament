// Copyright 1998-2016 Epic Games, Inc. All Rights Reservekd.

#include "DetailCustomizationsPrivatePCH.h"
#include "Runtime/Engine/Classes/Animation/PoseAsset.h"
#include "AnimationAssetDetails.h"

#define LOCTEXT_NAMESPACE	"AnimationAssetDetails"


TSharedRef<IDetailCustomization> FAnimationAssetDetails::MakeInstance()
{
	return MakeShareable(new FAnimationAssetDetails);
}

void FAnimationAssetDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr<UObject> > SelectedObjectsList = DetailBuilder.GetDetailsView().GetSelectedObjects();

	for (auto SelectionIt = SelectedObjectsList.CreateIterator(); SelectionIt; ++SelectionIt)
	{
		if (UAnimationAsset* TestAsset = Cast<UAnimationAsset>(SelectionIt->Get()))
		{
			if (TargetSkeleton.IsValid() && TestAsset->GetSkeleton() != TargetSkeleton.Get())
			{
				TargetSkeleton = nullptr;
				break;
			}
			else
			{
				TargetSkeleton = TestAsset->GetSkeleton();
			}
		}
	}

	/////////////////////////////////
	// Source Animation filter for skeleton
	PreviewPoseAssetHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimationAsset, PreviewPoseAsset));

	// add widget for editing source animation 
	IDetailCategoryBuilder& AnimationCategory = DetailBuilder.EditCategory("Animation");
	AnimationCategory.AddCustomRow(PreviewPoseAssetHandler->GetPropertyDisplayName())
	.NameContent()
	[
		PreviewPoseAssetHandler->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200.f)
	[
		SNew(SObjectPropertyEntryBox)
		.AllowedClass(UPoseAsset::StaticClass())
		.OnShouldFilterAsset(this, &FAnimationAssetDetails::ShouldFilterAsset)
		.PropertyHandle(PreviewPoseAssetHandler)
	];

	DetailBuilder.HideProperty(PreviewPoseAssetHandler);
}

void FAnimationAssetDetails::OnPreviewPoseAssetChanged(const FAssetData& AssetData)
{
	ensureAlways(PreviewPoseAssetHandler->SetValue(AssetData) == FPropertyAccess::Result::Success);;
}

bool FAnimationAssetDetails::ShouldFilterAsset(const FAssetData& AssetData)
{
	if (TargetSkeleton.IsValid())
	{
		FString SkeletonString = FAssetData(TargetSkeleton.Get()).GetExportTextName();
		const FString* Value = AssetData.TagsAndValues.Find(TEXT("Skeleton"));
		return (!Value || SkeletonString != *Value);
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
