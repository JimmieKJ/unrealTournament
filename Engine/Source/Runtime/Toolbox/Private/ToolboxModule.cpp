// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ToolboxModule.h"
#include "SlateBasics.h"
#include "ModuleManager.h"
#include "GammaUI.h"
#include "ModuleUIInterface.h"
#include "MainFrame.h"
#include "DesktopPlatformModule.h"
#include "SDockTab.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "STestSuite.h"
#include "ISlateReflectorModule.h"


static bool bTabsRegistered = false;

static bool CanShowModulesTab()
{
	FString SolutionPath;
	return FDesktopPlatformModule::Get()->GetSolutionPath(SolutionPath);
}

class SDebugPanel : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDebugPanel){}
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& )
	{
		ChildSlot
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 4.0f )
			.HAlign(HAlign_Left)
			[
				SNew( SButton )
				.Text( NSLOCTEXT("DeveloperToolbox", "ReloadTextures", "Reload Textures") )
				.OnClicked( this, &SDebugPanel::OnReloadTexturesClicked )
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 4.0f )
			.HAlign(HAlign_Left)
			[
				SNew( SButton )
				.Text( NSLOCTEXT("DeveloperToolbox", "FlushFontCache", "Flush Font Cache") )
				.OnClicked( this, &SDebugPanel::OnFlushFontCacheClicked )
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 4.0f )
			.HAlign(HAlign_Left)
			[
				SNew( SButton )
				.Text( NSLOCTEXT("DeveloperToolbox", "TestSuite", "Test Suite") )
				.OnClicked( this, &SDebugPanel::OnTestSuiteClicked )
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 4.0f )
			.HAlign(HAlign_Left)
			[
				SNew( SButton )
				.Text( NSLOCTEXT("DeveloperToolbox", "DisplayTextureAtlases", "Display Texture Atlases") )
				.OnClicked( this, &SDebugPanel::OnDisplayTextureAtlases )
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 4.0f )
			.HAlign(HAlign_Left)
			[
				SNew( SButton )
				.Text( NSLOCTEXT("DeveloperToolbox", "DisplayFontAtlases", "Display Font Atlases") )
				.OnClicked( this, &SDebugPanel::OnDisplayFontAtlases )
			]
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	
	FReply OnReloadTexturesClicked()
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
		return FReply::Handled();
	}

	FReply OnDisplayTextureAtlases()
	{
		static const FName SlateReflectorModuleName("SlateReflector");
		FModuleManager::LoadModuleChecked<ISlateReflectorModule>(SlateReflectorModuleName).DisplayTextureAtlasVisualizer();
		return FReply::Handled();
	}

	FReply OnDisplayFontAtlases()
	{
		static const FName SlateReflectorModuleName("SlateReflector");
		FModuleManager::LoadModuleChecked<ISlateReflectorModule>(SlateReflectorModuleName).DisplayFontAtlasVisualizer();
		return FReply::Handled();
	}

	FReply OnFlushFontCacheClicked()
	{
		FSlateApplication::Get().GetRenderer()->FlushFontCache();
		return FReply::Handled();
	}

	FReply OnTestSuiteClicked()
	{
		RestoreSlateTestSuite();
		return FReply::Handled();
	}
};

TSharedRef<SDockTab> CreateDebugToolsTab( const FSpawnTabArgs& Args )
{
	FGammaUI& GammaUI = FModuleManager::Get().LoadModuleChecked<FGammaUI>( FName("GammaUI") );

	return
		SNew(SDockTab)
		.TabRole( ETabRole::NomadTab )
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SDebugPanel )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				GammaUI.GetGammaUIPanel().ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> CreateModulesTab( const FSpawnTabArgs& Args )
{
	IModuleUIInterface& ModuleUI = FModuleManager::Get().LoadModuleChecked<IModuleUIInterface>( FName("ModuleUI") );

	return
		SNew(SDockTab)
		.TabRole( ETabRole::NomadTab )
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			[
				ModuleUI.GetModuleUIWidget()
			]
		];
}



class FToolboxModule : public IToolboxModule
{
	void StartupModule() override
	{

	}

	void ShutdownModule() override
	{
		bTabsRegistered = false;

		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("DebugTools");
			if (CanShowModulesTab())
			{
				FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("ModulesTab");
			}
		}
	}

	virtual void RegisterSpawners(const TSharedPtr<FWorkspaceItem>& DebugToolsTabCategory, const TSharedPtr<FWorkspaceItem>& ModulesTabCategory) override
	{
		if (!bTabsRegistered)
		{
			bTabsRegistered = true;
			{
				FTabSpawnerEntry& Spawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner("DebugTools", FOnSpawnTab::CreateStatic(&CreateDebugToolsTab))
					.SetDisplayName(NSLOCTEXT("Toolbox", "DebugTools", "Debug Tools"))
					.SetTooltipText(NSLOCTEXT("Toolbox", "DebugToolsTooltipText", "Open the Debug Tools tab."))
					.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "DebugTools.TabIcon"));

				if (DebugToolsTabCategory.IsValid())
				{
					Spawner.SetGroup(DebugToolsTabCategory.ToSharedRef());
				}
			}

			if (CanShowModulesTab())
			{
				FTabSpawnerEntry& Spawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner("ModulesTab", FOnSpawnTab::CreateStatic(&CreateModulesTab))
					.SetDisplayName(NSLOCTEXT("Toolbox", "Modules", "Modules"))
					.SetTooltipText(NSLOCTEXT("Toolbox", "ModulesTooltipText", "Open the Modules tab."))
					.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Modules.TabIcon"));

				if (ModulesTabCategory.IsValid())
				{
					Spawner.SetGroup(ModulesTabCategory.ToSharedRef());
				}
			}
		}
	}

	virtual void SummonToolbox() override
	{
		RegisterSpawners(nullptr, nullptr);
		FGlobalTabmanager::Get()->InvokeTab(FTabId("DebugTools"));
	}
};


IMPLEMENT_MODULE(FToolboxModule, Toolbox);

