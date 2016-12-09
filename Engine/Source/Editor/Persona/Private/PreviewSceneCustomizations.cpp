// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PreviewSceneCustomizations.h"
#include "Modules/ModuleManager.h"
#include "AssetData.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PersonaPreviewSceneDescription.h"
#include "Engine/PreviewMeshCollection.h"
#include "Factories/PreviewMeshCollectionFactory.h"
#include "IPropertyUtilities.h"
#include "Preferences/PersonaOptions.h"

#define LOCTEXT_NAMESPACE "PreviewSceneCustomizations"

void FPreviewSceneDescriptionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TSharedRef<IPropertyHandle> AnimationModeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, AnimationMode));
	AnimationModeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPreviewSceneDescriptionCustomization::HandleAnimationModeChanged));
	TSharedRef<IPropertyHandle> AnimationProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, Animation));
	TSharedRef<IPropertyHandle> SkeletalMeshProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, PreviewMesh));
	TSharedRef<IPropertyHandle> AdditionalMeshesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPersonaPreviewSceneDescription, AdditionalMeshes));

	DetailBuilder.EditCategory("Animation")
	.AddProperty(AnimationModeProperty);

	DetailBuilder.EditCategory("Animation")
	.AddProperty(AnimationProperty)
	.CustomWidget()
	.NameContent()
	[
		AnimationProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(250.0f)
	.MinDesiredWidth(250.0f)
	[
		SNew(SObjectPropertyEntryBox)
		.AllowedClass(UAnimationAsset::StaticClass())
		.PropertyHandle(AnimationProperty)
		.OnShouldFilterAsset(this, &FPreviewSceneDescriptionCustomization::HandleShouldFilterAsset, false)
		.OnObjectChanged(this, &FPreviewSceneDescriptionCustomization::HandleAnimationChanged)
		.ThumbnailPool(DetailBuilder.GetThumbnailPool())
	];

	if (PersonaToolkit.Pin()->GetContext() != USkeletalMesh::StaticClass()->GetFName())
	{
		FText PreviewMeshName;
		if (PersonaToolkit.Pin()->GetContext() == UAnimationAsset::StaticClass()->GetFName())
		{
			PreviewMeshName = FText::Format(LOCTEXT("PreviewMeshAnimation", "{0} (Animation)"), SkeletalMeshProperty->GetPropertyDisplayName());
		}
		else
		{
			PreviewMeshName = FText::Format(LOCTEXT("PreviewMeshSkeleton", "{0} (Skeleton)"), SkeletalMeshProperty->GetPropertyDisplayName());
		}

		DetailBuilder.EditCategory("Mesh")
		.AddProperty(SkeletalMeshProperty)
		.CustomWidget()
		.NameContent()
		[
			SkeletalMeshProperty->CreatePropertyNameWidget(PreviewMeshName)
		]
		.ValueContent()
		.MaxDesiredWidth(250.0f)
		.MinDesiredWidth(250.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.PropertyHandle(SkeletalMeshProperty)
			.OnShouldFilterAsset(this, &FPreviewSceneDescriptionCustomization::HandleShouldFilterAsset, false)
			.OnObjectChanged(this, &FPreviewSceneDescriptionCustomization::HandleMeshChanged)
			.ThumbnailPool(DetailBuilder.GetThumbnailPool())
		];
	}
	else
	{
		DetailBuilder.HideProperty(SkeletalMeshProperty);
	}

	// use a factory with a custom fixed skeleton as we shouldn't be picking one here
	UPreviewMeshCollectionFactory* FactoryToUse = NewObject<UPreviewMeshCollectionFactory>();
	FactoryToUse->CurrentSkeleton = &EditableSkeleton.Pin()->GetSkeleton();

	TArray<UFactory*> FactoriesToUse({ FactoryToUse });

	DetailBuilder.EditCategory("Additional Meshes")
	.AddProperty(AdditionalMeshesProperty)
	.CustomWidget()
	.NameContent()
	[
		AdditionalMeshesProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(250.0f)
	.MinDesiredWidth(250.0f)
	[
		SNew(SObjectPropertyEntryBox)
		.AllowedClass(UPreviewMeshCollection::StaticClass())
		.PropertyHandle(AdditionalMeshesProperty)
		.OnShouldFilterAsset(this, &FPreviewSceneDescriptionCustomization::HandleShouldFilterAsset, true)
		.OnObjectChanged(this, &FPreviewSceneDescriptionCustomization::HandleAdditionalMeshesChanged, &DetailBuilder)
		.ThumbnailPool(DetailBuilder.GetThumbnailPool())
		.NewAssetFactories(FactoriesToUse)
	];

	FAssetData AdditionalMeshesAsset;
	AdditionalMeshesProperty->GetValue(AdditionalMeshesAsset);

	if (AdditionalMeshesAsset.IsValid())
	{
		TArray<UObject*> Objects;
		Objects.Add(AdditionalMeshesAsset.GetAsset());

		IDetailPropertyRow* PropertyRow = DetailBuilder.EditCategory("Additional Meshes")
		.AddExternalProperty(Objects, "SkeletalMeshes");

		if (PropertyRow)
		{
			PropertyRow->ShouldAutoExpand(true);
		}
	}
}

bool FPreviewSceneDescriptionCustomization::HandleShouldFilterAsset(const FAssetData& InAssetData, bool bCanUseDifferentSkeleton)
{
	if (bCanUseDifferentSkeleton && GetDefault<UPersonaOptions>()->bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons)
	{
		return false;
	}

	FString SkeletonTag = InAssetData.GetTagValueRef<FString>("Skeleton");
	if (SkeletonTag == SkeletonName)
	{
		return false;
	}

	return true;
}

void FPreviewSceneDescriptionCustomization::HandleAnimationModeChanged()
{
	UPersonaPreviewSceneDescription* PersonaPreviewSceneDescription = PreviewScene.Pin()->GetPreviewSceneDescription();
	switch (PersonaPreviewSceneDescription->AnimationMode)
	{
	case EPreviewAnimationMode::Default:
		PreviewScene.Pin()->ShowDefaultMode();
		break;
	case EPreviewAnimationMode::ReferencePose:
		PreviewScene.Pin()->ShowReferencePose(true);
		break;
	case EPreviewAnimationMode::UseSpecificAnimation:
		PreviewScene.Pin()->SetPreviewAnimationAsset(Cast<UAnimationAsset>(PersonaPreviewSceneDescription->Animation.LoadSynchronous()));
		break;
	}
}

void FPreviewSceneDescriptionCustomization::HandleAnimationChanged(const FAssetData& InAssetData)
{
	UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(InAssetData.GetAsset());
	PreviewScene.Pin()->SetPreviewAnimationAsset(AnimationAsset);

	if (AnimationAsset != nullptr)
	{
		PreviewScene.Pin()->GetPreviewSceneDescription()->AnimationMode = EPreviewAnimationMode::UseSpecificAnimation;
	}
}

void FPreviewSceneDescriptionCustomization::HandleMeshChanged(const FAssetData& InAssetData)
{
	USkeletalMesh* NewPreviewMesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
	PersonaToolkit.Pin()->SetPreviewMesh(NewPreviewMesh);
}

void FPreviewSceneDescriptionCustomization::HandleAdditionalMeshesChanged(const FAssetData& InAssetData, IDetailLayoutBuilder* DetailLayoutBuilder)
{
	PreviewScene.Pin()->SetAdditionalMeshes(Cast<UPreviewMeshCollection>(InAssetData.GetAsset()));
	DetailLayoutBuilder->ForceRefreshDetails();
}

void FPreviewMeshCollectionEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// get the enclosing preview mesh collection to determine the skeleton we want
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	check(OuterObjects.Num() > 0);
		
	if (OuterObjects[0] != nullptr)
	{
		FString SkeletonName = FAssetData(CastChecked<UPreviewMeshCollection>(OuterObjects[0])->Skeleton).GetExportTextName();

		PropertyHandle->GetParentHandle()->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPreviewMeshCollectionEntryCustomization::HandleMeshesArrayChanged, CustomizationUtils.GetPropertyUtilities()));

		TSharedPtr<IPropertyHandle> SkeletalMeshProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPreviewMeshCollectionEntry, SkeletalMesh));
		if (SkeletalMeshProperty.IsValid())
		{
			HeaderRow.NameContent()
			[
				SkeletalMeshProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(250.0f)
			.MinDesiredWidth(250.0f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(USkeletalMesh::StaticClass())
				.PropertyHandle(SkeletalMeshProperty)
				.OnShouldFilterAsset(this, &FPreviewMeshCollectionEntryCustomization::HandleShouldFilterAsset, SkeletonName)
				.OnObjectChanged(this, &FPreviewMeshCollectionEntryCustomization::HandleMeshChanged)
				.ThumbnailPool(CustomizationUtils.GetThumbnailPool())
			];
		}
	}
}

bool FPreviewMeshCollectionEntryCustomization::HandleShouldFilterAsset(const FAssetData& InAssetData, FString SkeletonName)
{
	if (GetDefault<UPersonaOptions>()->bAllowPreviewMeshCollectionsToSelectFromDifferentSkeletons)
	{
		return false;
	}

	FString SkeletonTag = InAssetData.GetTagValueRef<FString>("Skeleton");
	if (SkeletonTag == SkeletonName)
	{
		return false;
	}

	return true;
}

void FPreviewMeshCollectionEntryCustomization::HandleMeshChanged(const FAssetData& InAssetData)
{
	if (PreviewScene.IsValid())
	{
		PreviewScene.Pin()->RefreshAdditionalMeshes();
	}
}

void FPreviewMeshCollectionEntryCustomization::HandleMeshesArrayChanged(TSharedPtr<IPropertyUtilities> PropertyUtilities)
{
	if (PreviewScene.IsValid())
	{
		PreviewScene.Pin()->RefreshAdditionalMeshes();
		if (PropertyUtilities.IsValid())
		{
			PropertyUtilities->ForceRefresh();
		}
	}
}

#undef LOCTEXT_NAMESPACE
