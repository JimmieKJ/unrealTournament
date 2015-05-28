// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationDashboardPrivatePCH.h"
#include "LocalizationDashboard.h"
#include "SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IMainFrameModule.h"
#include "ModuleManager.h"
#include "SLocalizationTargetEditor.h"
#include "LocalizationDashboardSettings.h"
#include "LocalizationTargetTypes.h"
#include "SSettingsEditorCheckoutNotice.h"

#define LOCTEXT_NAMESPACE "LocalizationDashboard"

const FName FLocalizationDashboard::TabName("LocalizationDashboard");

class SLocalizationDashboard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLocalizationDashboard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<SWindow>& OwningWindow, const TSharedRef<SDockTab>& OwningTab);

	TWeakPtr<SDockTab> ShowTargetEditor(ULocalizationTarget* const LocalizationTarget);

private:
	bool CanMakeEdits() const;

private:
	static const FName EngineTargetSetDetailsTabName;
	static const FName ProjectTargetSetDetailsTabName;
	static const FName DocumentsTabName;

	TSharedPtr<FTabManager> TabManager;
	TMap< TWeakObjectPtr<ULocalizationTarget>, TWeakPtr<SDockTab> > TargetToTabMap;
	TSharedPtr<SSettingsEditorCheckoutNotice> SettingsEditorCheckoutNotice;
};

const FName SLocalizationDashboard::EngineTargetSetDetailsTabName("EngineTargets");
const FName SLocalizationDashboard::ProjectTargetSetDetailsTabName("ProjectTargets");
const FName SLocalizationDashboard::DocumentsTabName("Documents");

void SLocalizationDashboard::Construct(const FArguments& InArgs, const TSharedPtr<SWindow>& OwningWindow, const TSharedRef<SDockTab>& OwningTab)
{
	TabManager = FGlobalTabmanager::Get()->NewTabManager(OwningTab);

	const auto& PersistLayout = [](const TSharedRef<FTabManager::FLayout>& LayoutToSave)
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
	};
	TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateLambda(PersistLayout));

	const auto& CreateDetailsTab = [this](const FSpawnTabArgs& SpawnTabArgs) -> TSharedRef<SDockTab>
	{
		FText DockTabLabel;
		ULocalizationTargetSet* TargetSet = nullptr;
		if (SpawnTabArgs.GetTabId() == EngineTargetSetDetailsTabName)
		{
			DockTabLabel = LOCTEXT("EngineTargetsTabLabel", "Engine Targets");
			TargetSet = ULocalizationDashboardSettings::GetEngineTargetSet();
		}
		else if (SpawnTabArgs.GetTabId() == ProjectTargetSetDetailsTabName)
		{
			DockTabLabel = LOCTEXT("ProjectTargetsTabLabel", "Project Targets");
			TargetSet = ULocalizationDashboardSettings::GetGameTargetSet();
		}

		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.Label(DockTabLabel);

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs(false, false, false, FDetailsViewArgs::ENameAreaSettings::HideNameArea, false, nullptr, false, NAME_None);
		TSharedRef<IDetailsView> DetailsView = PropertyModule.CreateDetailView(DetailsViewArgs);
		DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &SLocalizationDashboard::CanMakeEdits));

		DetailsView->SetObject(TargetSet, true);

		DockTab->SetContent(DetailsView);

		return DockTab;
	};
	const TSharedRef<FWorkspaceItem> TargetSetsWorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("TargetSetsWorkspaceMenuCategory", "Target Sets"));
	FTabSpawnerEntry& EngineTargetSetTabSpawnerEntry = TabManager->RegisterTabSpawner(EngineTargetSetDetailsTabName, FOnSpawnTab::CreateLambda(CreateDetailsTab))
		.SetDisplayName(LOCTEXT("EngineTargetsDetailTabSpawner", "Engine Targets"));
	TargetSetsWorkspaceMenuCategory->AddItem(EngineTargetSetTabSpawnerEntry.AsShared());
	FTabSpawnerEntry& ProjectTargetSetTabSpawnerEntry = TabManager->RegisterTabSpawner(ProjectTargetSetDetailsTabName, FOnSpawnTab::CreateLambda(CreateDetailsTab))
		.SetDisplayName(LOCTEXT("ProjectTargetsDetailTabSpawner", "Project Targets"));
	TargetSetsWorkspaceMenuCategory->AddItem(ProjectTargetSetTabSpawnerEntry.AsShared());

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("LocalizationDashboard_Experimental_V4")
		->AddArea
		(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
		FTabManager::NewStack()
		->SetHideTabWell(true)
		->AddTab(EngineTargetSetDetailsTabName, ETabState::ClosedTab)
		->AddTab(ProjectTargetSetDetailsTabName, ETabState::OpenedTab)
		)
		->Split
		(
		FTabManager::NewStack()
		->SetHideTabWell(false)
		->AddTab(DocumentsTabName, ETabState::ClosedTab)
		)
		);

	Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout);

	const TSharedRef<FExtender> MenuExtender = MakeShareable(new FExtender());
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
	const TSharedRef<SWidget> MenuWidget = MainFrameModule.MakeMainMenu( TabManager, MenuExtender );

	FString ConfigFilePath;
	ConfigFilePath = GetDefault<ULocalizationDashboardSettings>()->GetDefaultConfigFilename();
	ConfigFilePath = FPaths::ConvertRelativePathToFull(ConfigFilePath);

	ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				MenuWidget
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f)
			[
				SAssignNew(SettingsEditorCheckoutNotice, SSettingsEditorCheckoutNotice)
				.ConfigFilePath(ConfigFilePath)
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				TabManager->RestoreFrom(Layout, OwningWindow).ToSharedRef()
			]
		];
}

TWeakPtr<SDockTab> SLocalizationDashboard::ShowTargetEditor(ULocalizationTarget* const LocalizationTarget)
{
	// Create tab if not existent.
	TWeakPtr<SDockTab>& TargetEditorDockTab = TargetToTabMap.FindOrAdd(TWeakObjectPtr<ULocalizationTarget>(LocalizationTarget));

	if (!TargetEditorDockTab.IsValid())
	{
		ULocalizationTargetSet* const TargetSet = LocalizationTarget->GetTypedOuter<ULocalizationTargetSet>();

		const TSharedRef<SLocalizationTargetEditor> OurTargetEditor = SNew(SLocalizationTargetEditor, TargetSet, LocalizationTarget, FIsPropertyEditingEnabled::CreateSP(this, &SLocalizationDashboard::CanMakeEdits));
		const TSharedRef<SDockTab> NewTargetEditorTab = SNew(SDockTab)
			.TabRole(ETabRole::DocumentTab)
			.Label_Lambda( [LocalizationTarget]
			{
				return LocalizationTarget ? FText::FromString(LocalizationTarget->Settings.Name) : FText::GetEmpty();
			})
			[
				OurTargetEditor
			];

		TabManager->InsertNewDocumentTab(DocumentsTabName, FTabManager::ESearchPreference::RequireClosedTab, NewTargetEditorTab);
		TargetEditorDockTab = NewTargetEditorTab;
	}
	else
	{
		const TSharedPtr<SDockTab> OldTargetEditorTab = TargetEditorDockTab.Pin();
		TabManager->DrawAttention(OldTargetEditorTab.ToSharedRef());
	}

	return TargetEditorDockTab;
}

bool SLocalizationDashboard::CanMakeEdits() const
{
	return SettingsEditorCheckoutNotice.IsValid() && SettingsEditorCheckoutNotice->IsUnlocked();
}

FLocalizationDashboard* FLocalizationDashboard::Instance = nullptr;

FLocalizationDashboard* FLocalizationDashboard::Get()
{
	return Instance;
}

void FLocalizationDashboard::Initialize()
{
	if (!Instance)
	{
		Instance = new FLocalizationDashboard();
	}
}

void FLocalizationDashboard::Terminate()
{
	if (Instance)
	{
		delete Instance;
	}
}

void FLocalizationDashboard::Show()
{
	FGlobalTabmanager::Get()->InvokeTab(TabName);
}

TWeakPtr<SDockTab> FLocalizationDashboard::ShowTargetEditorTab(ULocalizationTarget* const LocalizationTarget)
{
	if (LocalizationDashboardWidget.IsValid())
	{
		return LocalizationDashboardWidget->ShowTargetEditor(LocalizationTarget);
	}
	return nullptr;
}

FLocalizationDashboard::FLocalizationDashboard()
{
	RegisterTabSpawner();
}

FLocalizationDashboard::~FLocalizationDashboard()
{
	UnregisterTabSpawner();
}

void FLocalizationDashboard::RegisterTabSpawner()
{
	const auto& SpawnMainTab = [this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.Label(LOCTEXT("MainTabTitle", "Localization Dashboard"))
			.TabRole(ETabRole::MajorTab);

		DockTab->SetContent( SAssignNew(LocalizationDashboardWidget, SLocalizationDashboard, Args.GetOwnerWindow(), DockTab) );

		return DockTab;
	};

	FGlobalTabmanager::Get()->RegisterTabSpawner(TabName, FOnSpawnTab::CreateLambda( TFunction<TSharedRef<SDockTab> (const FSpawnTabArgs&)>(SpawnMainTab) ) )
		.SetDisplayName(LOCTEXT("MainTabTitle", "Localization Dashboard"));
}

void FLocalizationDashboard::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterTabSpawner(TabName);
}

#undef LOCTEXT_NAMESPACE