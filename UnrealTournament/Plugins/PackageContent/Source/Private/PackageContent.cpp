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
		ActionList->MapAction(Commands.OpenPackageContent, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageContentWindow));
	}
}

void FPackageContent::OpenPackageContentWindow()
{
	UE_LOG(LogPackageContent, Log, TEXT("Thanks for pressing that button, this feature coming soon!"));
}

void FPackageContent::CreatePackageContentMenu(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FPackageContentCommands::Get().OpenPackageContent, NAME_None, LOCTEXT("PackageContent", "Package"), TAttribute<FText>(), FSlateIcon(FPackageContentStyle::GetStyleSetName(), "PackageContent.PackageContent"), "LevelToolbarContent");
}


#undef LOCTEXT_NAMESPACE