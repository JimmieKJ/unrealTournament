// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "SourceCodeNavigation.h"
#include "GameProjectGenerationModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

bool FAssetTypeActions_Class::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_Class::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	UClass *const BaseClass = (InObjects.Num() == 1) ? Cast<UClass>(InObjects[0]) : nullptr;

	// Only allow the New class option if we have a base class that we can actually derive from in one of our project modules
	FGameProjectGenerationModule& GameProjectGenerationModule = FGameProjectGenerationModule::Get();
	TArray<FModuleContextInfo> ProjectModules = GameProjectGenerationModule.GetCurrentProjectModules();
	const bool bIsValidBaseClass = BaseClass && GameProjectGenerationModule.IsValidBaseClassForCreation(BaseClass, ProjectModules);

	auto CreateCreateDerivedClass = [BaseClass]()
	{
		// Work out where the header file for the current class is, as we'll use that path as the default for the new class
		FString BaseClassPath;
		if(FSourceCodeNavigation::FindClassHeaderPath(BaseClass, BaseClassPath))
		{
			// Strip off the actual filename as we only need the path
			BaseClassPath = FPaths::GetPath(BaseClassPath);
		}

		FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog(
			FAddToProjectConfig()
			.ParentClass(BaseClass)
			.InitialPath(BaseClassPath)
			.ParentWindow(FGlobalTabmanager::Get()->GetRootWindow())
		);
	};

	auto CanCreateDerivedClass = [bIsValidBaseClass]() -> bool
	{
		return bIsValidBaseClass;
	};

	FText NewDerivedClassClassLabel;
	FText NewDerivedClassClassToolTip;
	if(InObjects.Num() == 1)
	{
		const FText BaseClassName = FText::FromName(BaseClass->GetFName());
		NewDerivedClassClassLabel = FText::Format(LOCTEXT("Class_NewDerivedClassLabel_CreateFrom", "Create class derived from {0}"), BaseClassName);
		if(bIsValidBaseClass)
		{
			NewDerivedClassClassToolTip = FText::Format(LOCTEXT("Class_NewDerivedClassTooltip_CreateFrom", "Create a new class deriving from {0}."), BaseClassName);
		}
		else
		{
			NewDerivedClassClassToolTip = FText::Format(LOCTEXT("Class_NewDerivedClassTooltip_InvalidClass", "Cannot create a new class deriving from {0}."), BaseClassName);
		}
	}
	else
	{
		NewDerivedClassClassLabel = LOCTEXT("Class_NewDerivedClassLabel_InvalidNumberOfBases", "New class derived from...");
		NewDerivedClassClassToolTip = LOCTEXT("Class_NewDerivedClassTooltip_InvalidNumberOfBases", "Can only create a derived class when there is a single base class selected.");
	}

	MenuBuilder.AddMenuEntry(
		NewDerivedClassClassLabel,
		NewDerivedClassClassToolTip,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.AddCodeToProject"),
		FUIAction(
			FExecuteAction::CreateLambda(CreateCreateDerivedClass),
			FCanExecuteAction::CreateLambda(CanCreateDerivedClass)
			)
		);
}

UThumbnailInfo* FAssetTypeActions_Class::GetThumbnailInfo(UObject* Asset) const
{
	// todo: jdale - CLASS - We need to generate and store proper thumbnail info for classes so that we can store their custom render transforms
	// This can't be stored in the UClass instance (like we do for Blueprints), so we'll need another place to store it
	// This will need to be accessible to FClassThumbnailScene::GetSceneThumbnailInfo
	return nullptr;
}

void FAssetTypeActions_Class::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	for(UObject* Object : InObjects)
	{
		UClass* const Class = Cast<UClass>(Object);
		if(Class)
		{
			FString ClassHeaderPath;
			if(FSourceCodeNavigation::FindClassHeaderPath(Class, ClassHeaderPath))
			{
				const FString AbsoluteHeaderPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ClassHeaderPath);
				FSourceCodeNavigation::OpenSourceFile(AbsoluteHeaderPath);
			}
		}
	}
}

TWeakPtr<IClassTypeActions> FAssetTypeActions_Class::GetClassTypeActions(const FAssetData& AssetData) const
{
	UClass* Class = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetName.ToString());
	if(Class)
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		return AssetToolsModule.Get().GetClassTypeActionsForClass(Class);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
