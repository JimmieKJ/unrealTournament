// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnitTestFrameworkEditorPCH.h"
#include "UnitTestFrameworkEditorModule.h"
#include "Engine/World.h"
#include "UTFUnitTestInterface.h"
#include "UTFEditorAutomationTests.h"
#include "BlueprintEditorModule.h"
#include "UTFEditorStyle.h"
#include "MultiBoxBuilder.h" // for FExtender/FToolBarBuilder
#include "SlateIcon.h"
#include "BlueprintEditor.h" // for RegenerateMenusAndToolbars

#define LOCTEXT_NAMESPACE "UTFEditorModule"

/*******************************************************************************
 * 
 ******************************************************************************/

namespace UnitTestFrameworkEditorModuleImpl
{
	/**  */
	static void AddUnitTestingRegistryTags(const UWorld* World, TArray<UObject::FAssetRegistryTag>& TagsOut);

	/**  */
	static void ExtendBlueprintEditorMenus(TSharedPtr<FExtender> MenuExtender, UBlueprint* Blueprint);

	/**  */
	static FBlueprintEditorModule& GetBlueprintEditorModule();

	/**  */
	static void ExtendBlueprintToolbar(FToolBarBuilder& MenuBuilder, UBlueprint* Blueprint);
	
	/**  */
	TSharedRef<SWidget> GenerateBlueprintMenuContent(UBlueprint* Blueprint);
}

//------------------------------------------------------------------------------
static void UnitTestFrameworkEditorModuleImpl::AddUnitTestingRegistryTags(const UWorld* World, TArray<UObject::FAssetRegistryTag>& TagsOut)
{
	if (ULevel* Level = World->PersistentLevel)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (Actor == nullptr)
			{
				continue;
			}

			UClass* ActorClass = Actor->GetClass();
			if (FUTFEditorAutomationTests::IsClassSuitableForInEditorTesting(ActorClass))
			{
				TagsOut.Add(UObject::FAssetRegistryTag(FUTFEditorAutomationTests::UnitTestLevelTag, TEXT("TRUE"), UObject::FAssetRegistryTag::TT_Hidden));
			}
		}
	}
}

//------------------------------------------------------------------------------
static void UnitTestFrameworkEditorModuleImpl::ExtendBlueprintEditorMenus(TSharedPtr<FExtender> MenuExtender, UBlueprint* Blueprint)
{
	FToolBarExtensionDelegate ToolbarExtension = FToolBarExtensionDelegate::CreateStatic(ExtendBlueprintToolbar, Blueprint);

	FBlueprintEditorModule& BPEditor = UnitTestFrameworkEditorModuleImpl::GetBlueprintEditorModule();
	MenuExtender->AddToolBarExtension("Asset", EExtensionHook::After, BPEditor.GetsSharedBlueprintEditorCommands(), ToolbarExtension);
}

//------------------------------------------------------------------------------
static FBlueprintEditorModule& UnitTestFrameworkEditorModuleImpl::GetBlueprintEditorModule()
{
	return FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
}

//------------------------------------------------------------------------------
static void UnitTestFrameworkEditorModuleImpl::ExtendBlueprintToolbar(FToolBarBuilder& MenuBuilder, UBlueprint* Blueprint)
{
	auto HasUTFInterface = [Blueprint]()->bool
	{
		UClass* BlueprintClass = Blueprint->SkeletonGeneratedClass;
		return BlueprintClass && BlueprintClass->ImplementsInterface(UUTFUnitTestInterface::StaticClass());
	};

	//MenuBuilder.BeginSection(TEXT("UnitTestFramework"));
	{
		MenuBuilder.AddComboButton(
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction::CreateLambda(HasUTFInterface),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda(HasUTFInterface)
			),
			FOnGetContent::CreateStatic(GenerateBlueprintMenuContent, Blueprint),
			LOCTEXT("BlueprintOptionsButton", "Unit Test"),
			LOCTEXT("BlueprintOptionsButton_Tooltip", "Options to customize how this Blueprint is used with the UnitTestFramework."),
			FSlateIcon("UTFEditorStyle", "UTFBlueprint.SettingsIcon", "UTFBlueprint.SettingsIcon"),
			/*bSimpleComboBox =*/false
		);
	}
	//MenuBuilder.EndSection();
}

//------------------------------------------------------------------------------
TSharedRef<SWidget> UnitTestFrameworkEditorModuleImpl::GenerateBlueprintMenuContent(UBlueprint* Blueprint)
{
	FBlueprintEditorModule& BPEditor = UnitTestFrameworkEditorModuleImpl::GetBlueprintEditorModule();
	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection =*/false, BPEditor.GetsSharedBlueprintEditorCommands());

	auto ToggleEditorSafeTest = [Blueprint]()
	{
		UPackage* BlueprintPackage = Blueprint->GetOutermost();
		UMetaData* MetaData = BlueprintPackage->GetMetaData();

		if (MetaData->HasValue(Blueprint, FUTFEditorAutomationTests::InEditorTestTag))
		{
			MetaData->RemoveValue(Blueprint, FUTFEditorAutomationTests::InEditorTestTag);
		}
		else
		{
			MetaData->SetValue(Blueprint, FUTFEditorAutomationTests::InEditorTestTag, TEXT("TRUE"));
		}

		BlueprintPackage->MarkPackageDirty();
	};
	UClass* BlueprintClass = Blueprint->SkeletonGeneratedClass;

	MenuBuilder.AddMenuEntry(
		LOCTEXT("EditorSafeTest_Label", "In-Editor Test"),
		LOCTEXT("EditorSafeTest_Tooltip", "Flags this test as safe to run in the editor; meaning its script does not execute any actions that are prevented in the construction script (like SpawnActor, latent actions, etc.))"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(ToggleEditorSafeTest),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(FUTFEditorAutomationTests::IsClassSuitableForInEditorTesting, BlueprintClass)
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	return MenuBuilder.MakeWidget();
}

/*******************************************************************************
 * FUnitTestFrameworkEditorModule
 ******************************************************************************/

/** 
 * 
 */
class FUnitTestFrameworkEditorModule : public IUnitTestFrameworkEditorModuleInterface
{
public:
	//--------------------------------------------------------------------------
	virtual void StartupModule() override
	{
		FUTFEditorStyle::Initialize();
		GetWorldAssetTagsHandle = FWorldDelegates::GetAssetTags.AddStatic(UnitTestFrameworkEditorModuleImpl::AddUnitTestingRegistryTags);

		FBlueprintEditorModule& BPEditor = UnitTestFrameworkEditorModuleImpl::GetBlueprintEditorModule();
		BPEditor.OnGatherBlueprintMenuExtensions().AddStatic(UnitTestFrameworkEditorModuleImpl::ExtendBlueprintEditorMenus);
	}

	//--------------------------------------------------------------------------
	virtual void ShutdownModule() override
	{
		FWorldDelegates::GetAssetTags.Remove(GetWorldAssetTagsHandle);
		GetWorldAssetTagsHandle.Reset();

		FUTFEditorStyle::Shutdown();
	}

private: 
	FDelegateHandle GetWorldAssetTagsHandle;
};

IMPLEMENT_MODULE(FUnitTestFrameworkEditorModule, UnitTestFrameworkEditor);
#undef LOCTEXT_NAMESPACE
