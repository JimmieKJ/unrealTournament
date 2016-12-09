// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Customization/BlendSampleDetails.h"

#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "IDetailsView.h"
#include "EditorStyleSet.h"
#include "Misc/StringAssetReference.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SNumericEntryBox.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/BlendSpace1D.h"
#include "SAnimationBlendSpaceGridWidget.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "BlendSampleDetails"

FBlendSampleDetails::FBlendSampleDetails(const UBlendSpaceBase* InBlendSpace, class SBlendSpaceGridWidget* InGridWidget)
	: BlendSpace(InBlendSpace)
	, GridWidget(InGridWidget)
{
	// Retrieve the additive animation type enum
	const UEnum* AdditiveTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAdditiveAnimationType"), true);
	// For each type check whether or not the blend space is compatible with it and cache the result
	for (int32 Index = 0; Index < (int32)EAdditiveAnimationType::AAT_MAX; ++Index)
	{
		EAdditiveAnimationType Type = (EAdditiveAnimationType)Index;		
		// In case of non additive type make sure the blendspace is made up out of non additive samples only
		const bool bAdditiveFlag = (Type == EAdditiveAnimationType::AAT_None) ? !BlendSpace->IsValidAdditive() : BlendSpace->IsValidAdditive() && BlendSpace->IsValidAdditiveType(Type);		
		bValidAdditiveTypes.Add(AdditiveTypeEnum->GetNameByIndex(Index).ToString(), bAdditiveFlag);
	}
}

void FBlendSampleDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	static const FName CategoryName = FName("BlendSample");
	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(CategoryName);

	// Hide default properties
	TArray<TSharedRef<IPropertyHandle> > DefaultProperties;
	CategoryBuilder.GetDefaultProperties(DefaultProperties);

	// Hide all default properties
	for (TSharedRef<IPropertyHandle> Property : DefaultProperties)
	{
		Property->MarkHiddenByCustomization();		
	}

	// For each Blend parameter generate a numeric entry box (similar to 
	const int32 NumParameters = BlendSpace->IsA<UBlendSpace1D>() ? 1 : 2;
	for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
	{
		const FBlendParameter& BlendParameter = BlendSpace->GetBlendParameter(ParameterIndex);
		FDetailWidgetRow& ParameterRow = CategoryBuilder.AddCustomRow(FText::FromString(BlendParameter.DisplayName));
		ParameterRow.NameContent()
		[
			SNew(STextBlock)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(FText::FromString(BlendParameter.DisplayName))
		];

		ParameterRow.ValueContent()
		[
			GridWidget->CreateGridEntryBox(ParameterIndex, false).ToSharedRef()
		];	
	}

	TSharedPtr<IPropertyHandle> AnimationProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FBlendSample, Animation), (UClass*)(FBlendSample::StaticStruct()));
	FDetailWidgetRow& AnimationRow = CategoryBuilder.AddCustomRow(FText::FromString(TEXT("Animation")));
	AnimationRow.NameContent()
	[
		SNew(STextBlock)
		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.Text(AnimationProperty->GetPropertyDisplayName())		
	];
	
	AnimationRow.ValueContent()
	.MinDesiredWidth(250.f)
	[
		SNew(SObjectPropertyEntryBox)
		.AllowedClass(UAnimSequence::StaticClass())
		.OnShouldFilterAsset(this, &FBlendSampleDetails::ShouldFilterAsset)
		.PropertyHandle(AnimationProperty)
	];
	
}

bool FBlendSampleDetails::ShouldFilterAsset(const FAssetData& AssetData) const
{
	bool bShouldFilter = true;

	// Skeleton is a private member so cannot use GET_MEMBER_NAME_CHECKED and friend class seemed unjustified to add
	const FName SkeletonTagName = "Skeleton";
	FString SkeletonName;
	if (AssetData.GetTagValue(SkeletonTagName, SkeletonName))
	{
		FStringAssetReference StringAssetReference(SkeletonName);
		// Check whether or not the skeletons match
		if (StringAssetReference.ToString() == BlendSpace->GetSkeleton()->GetPathName())
		{
			// If so check if the additive animation tpye is compatible with the blend space
			const FName AdditiveTypeTagName = GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType);
			FString AnimationTypeName;
			if (AssetData.GetTagValue(AdditiveTypeTagName, AnimationTypeName))
			{
				bShouldFilter = !bValidAdditiveTypes.FindChecked(AnimationTypeName);
			}
			else
			{
				// If the asset does not contain the requried tag value retrieve the asset and validate it
				const UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
				if (AnimSequence)
				{
					bShouldFilter = !(AnimSequence && BlendSpace->ValidateAnimationSequence(AnimSequence));
				}
			}
		}
	}
	
	return bShouldFilter;
}

#undef LOCTEXT_NAMESPACE
