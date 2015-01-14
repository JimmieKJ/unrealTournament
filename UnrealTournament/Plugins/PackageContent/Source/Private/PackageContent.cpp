// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PackageContentPrivatePCH.h"
#include "PackageContent.h"
#include "LevelEditor.h"
#include "PackageContentCommands.h"
#include "PackageContentStyle.h"

#define LOCTEXT_NAMESPACE "PackageContent"

DEFINE_LOG_CATEGORY(LogPackageContent);

TSharedRef< FPackageContent > FPackageContent::Create()
{
	TSharedRef< FUICommandList > ActionList = MakeShareable(new FUICommandList());

	TSharedRef< FPackageContent > PackageContent = MakeShareable(new FPackageContent(ActionList));
	PackageContent->Initialize();
	return PackageContent;
}

FPackageContent::FPackageContent(const TSharedRef< FUICommandList >& InActionList)
: ActionList(InActionList)
{

}

FPackageContent::~FPackageContent()
{
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(MenuExtender);
	}
}

void FPackageContent::Initialize()
{
	if (!IsRunningCommandlet())
	{
		MenuExtender = MakeShareable(new FExtender);

		MenuExtender->AddToolBarExtension(
			"Content",
			EExtensionHook::After,
			ActionList,
			FToolBarExtensionDelegate::CreateSP(this, &FPackageContent::CreatePackageContentMenu));

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(MenuExtender);

		FPackageContentCommands::Register();
		const FPackageContentCommands& Commands = FPackageContentCommands::Get();
		ActionList->MapAction(Commands.PackageLevel, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageLevelWindow));
		ActionList->MapAction(Commands.PackageWeapon, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageWeaponWindow));
		ActionList->MapAction(Commands.PackageHat, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageHatWindow));
	}
}

void FPackageContent::OpenPackageLevelWindow()
{
	UE_LOG(LogPackageContent, Log, TEXT("Thanks for pressing that button, this feature coming soon!"));
}

void FPackageContent::OpenPackageWeaponWindow()
{
	UE_LOG(LogPackageContent, Log, TEXT("Thanks for pressing that button, this feature coming soon!"));
}

void FPackageContent::OpenPackageHatWindow()
{
	UE_LOG(LogPackageContent, Log, TEXT("Thanks for pressing that button, this feature coming soon!"));
}

void FPackageContent::CreatePackageContentMenu(FToolBarBuilder& Builder)
{
	Builder.AddComboButton(FUIAction(),	FOnGetContent::CreateSP(this, &FPackageContent::GenerateOpenPackageMenuContent),
						   LOCTEXT("PackageContent_Label", "Package"), LOCTEXT("PackageContent_ToolTip", "Put your custom content into a package file to get it ready for marketplace."),
		                   FSlateIcon(FPackageContentStyle::GetStyleSetName(), "PackageContent.PackageContent"));
}

TSharedRef<SWidget> FPackageContent::GenerateOpenPackageMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, ActionList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("Package Content", "Package Content"));
	{
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageLevel);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageWeapon);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageHat);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE