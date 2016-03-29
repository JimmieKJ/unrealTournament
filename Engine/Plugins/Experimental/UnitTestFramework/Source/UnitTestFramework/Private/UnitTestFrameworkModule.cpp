// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnitTestFrameworkPCH.h"
#include "UnitTestFrameworkModule.h"

/** 
 * 
 */
class FUnitTestFrameworkModule : public IUnitTestFrameworkModuleInterface
{
public:
	virtual void StartupModule() override
	{
// 		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
// 		TArray<FContentBrowserMenuExtender_SelectedAssets> ContentBrowserExtensionManager = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
// 
// 
// 		FBlueprintEditorModule& BPEditor = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
// 		MenuBarExtender = MakeShareable(new FExtender);
// 
// 		TSharedPtr< FExtender > MenuBarExtender;
// 
// 		MenuBarExtender->AddToolBarExtension("Asset", EExtensionHook::After, BPEditor.GetsSharedBlueprintEditorCommands(), FToolBarExtensionDelegate::CreateStatic(AddMyToolbarExtension));
// 		BPEditor.GetMenuExtensibilityManager()->AddExtender(MenuBarExtender);
// 
// 		MenuBuilder.BeginSection("AssetContextAdvancedActions", LOCTEXT("AssetContextAdvancedActionsMenuHeading", "Advanced"));
	}

	virtual void ShutdownModule() override
	{
	}
};
IMPLEMENT_MODULE(FUnitTestFrameworkModule, UnitTestFramework);
