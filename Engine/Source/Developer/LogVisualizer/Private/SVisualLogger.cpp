// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "SDockTab.h"
#include "VisualLogger/VisualLogger.h"
#include "VisualLoggerRenderingActor.h"
#include "VisualLoggerCanvasRenderer.h"
#include "DesktopPlatformModule.h"
#include "MainFrame.h"
#include "VisualLoggerCameraController.h"
#if WITH_EDITOR
#	include "Editor/UnrealEd/Public/EditorComponents.h"
#	include "Editor/UnrealEd/Public/EditorReimportHandler.h"
#	include "Editor/UnrealEd/Public/TexAlignTools.h"
#	include "Editor/UnrealEd/Public/TickableEditorObject.h"
#	include "UnrealEdClasses.h"
#	include "Editor/UnrealEd/Public/Editor.h"
#	include "Editor/UnrealEd/Public/EditorViewportClient.h"
#endif
#include "ISettingsModule.h"

#include "VisualLogger/VisualLoggerBinaryFileDevice.h"

#define LOCTEXT_NAMESPACE "SVisualLogger"

/* Local constants
*****************************************************************************/
static const FName ToolbarTabId("Toolbar");
static const FName FiltersTabId("Filters");
static const FName MainViewTabId("MainView");
static const FName LogsListTabId("LogsList");
static const FName StatusViewTabId("StatusView");

namespace LogVisualizer
{
	static const FString LogFileDescription = LOCTEXT("FileTypeDescription", "Visual Log File").ToString();
	static const FString LoadFileTypes = FString::Printf(TEXT("%s (*.bvlog;*.%s)|*.bvlog;*.%s"), *LogFileDescription, VISLOG_FILENAME_EXT, VISLOG_FILENAME_EXT);
	static const FString SaveFileTypes = FString::Printf(TEXT("%s (*.%s)|*.%s"), *LogFileDescription, VISLOG_FILENAME_EXT, VISLOG_FILENAME_EXT);
}

SVisualLogger::FVisualLoggerDevice::FVisualLoggerDevice(SVisualLogger* InOwner)
	:Owner(InOwner)
{
}

SVisualLogger::FVisualLoggerDevice::~FVisualLoggerDevice() 
{
}

void SVisualLogger::FVisualLoggerDevice::Serialize(const class UObject* LogOwner, FName OwnerName, FName OwnerClassName, const FVisualLogEntry& LogEntry)
{
	if (Owner == NULL)
	{
		return;
	}

	UWorld* World = FLogVisualizer::Get().GetWorld();
	if (LastWorld.Get() != World)
	{
		Owner->OnNewWorld(LastWorld.Get());
		LastWorld = World;
	}

	Owner->OnNewLogEntry(FVisualLogDevice::FVisualLogEntryItem(OwnerName, OwnerClassName, LogEntry));
}

/* SMessagingDebugger constructors
*****************************************************************************/

SVisualLogger::SVisualLogger() : SCompoundWidget(), CommandList(MakeShareable(new FUICommandList))
{ 
	bPausedLogger = false;
	bGotHistogramData = false;
	InternalDevice = MakeShareable(new FVisualLoggerDevice(this));
	FVisualLogger::Get().AddDevice(InternalDevice.Get());
}

SVisualLogger::~SVisualLogger()
{
	UWorld* World = NULL;
#if WITH_EDITOR
	FCategoryFiltersManager::Get().SavePresistentData();

	UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
	if (GIsEditor && EEngine != NULL)
	{
		// lets use PlayWorld during PIE/Simulate and regular world from editor otherwise, to draw debug information
		World = EEngine->PlayWorld != NULL ? EEngine->PlayWorld : EEngine->GetEditorWorldContext().World();

	}
	else
#endif
	if (!GIsEditor)
	{
	World = GEngine->GetWorld();
	}

	if (World == NULL)
	{
		World = GWorld;
	}

	if (World)
	{
		for (TActorIterator<AVisualLoggerRenderingActor> It(World); It; ++It)
		{
			World->DestroyActor(*It);
		}
	}

	UDebugDrawService::Unregister(DrawOnCanvasDelegateHandle);
	VisualLoggerCanvasRenderer.Reset();

	FVisualLogger::Get().RemoveDevice(InternalDevice.Get());
	InternalDevice.Reset();
}

/* SMessagingDebugger interface
*****************************************************************************/

void SVisualLogger::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	bPausedLogger = false;
	bGotHistogramData = false;

	FLogVisualizer::Get().SetCurrentVisualizer(SharedThis(this));
	//////////////////////////////////////////////////////////////////////////
	// Visual Logger Events
	FLogVisualizer::Get().GetVisualLoggerEvents().OnItemSelectionChanged = FOnItemSelectionChanged::CreateRaw(this, &SVisualLogger::OnItemSelectionChanged);
	FLogVisualizer::Get().GetVisualLoggerEvents().OnFiltersChanged = FOnFiltersChanged::CreateRaw(this, &SVisualLogger::OnFiltersChanged);
	FLogVisualizer::Get().GetVisualLoggerEvents().OnObjectSelectionChanged = FOnObjectSelectionChanged::CreateRaw(this, &SVisualLogger::OnObjectSelectionChanged);
	FLogVisualizer::Get().GetVisualLoggerEvents().OnLogLineSelectionChanged = FOnLogLineSelectionChanged::CreateRaw(this, &SVisualLogger::OnLogLineSelectionChanged);

	//////////////////////////////////////////////////////////////////////////
	// Command Action Lists
	const FVisualLoggerCommands& Commands = FVisualLoggerCommands::Get();
	FUICommandList& ActionList = *CommandList;

	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	FCategoryFiltersManager::Get().LoadPresistentData();

	ActionList.MapAction(Commands.StartRecording, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandIsVisible));
	ActionList.MapAction(Commands.StopRecording, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandIsVisible));
	ActionList.MapAction(Commands.Pause, FExecuteAction::CreateRaw(this, &SVisualLogger::HandlePauseCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandlePauseCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandlePauseCommandIsVisible));
	ActionList.MapAction(Commands.Resume, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleResumeCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleResumeCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleResumeCommandIsVisible));
	ActionList.MapAction(Commands.LoadFromVLog, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleLoadCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleLoadCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleLoadCommandCanExecute));
	ActionList.MapAction(Commands.SaveToVLog, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute));
	ActionList.MapAction(Commands.FreeCamera, 
		FExecuteAction::CreateRaw(this, &SVisualLogger::HandleCameraCommandExecute), 
		FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleCameraCommandCanExecute), 
		FIsActionChecked::CreateRaw(this, &SVisualLogger::HandleCameraCommandIsChecked),
		FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleCameraCommandCanExecute));
	ActionList.MapAction(Commands.ToggleGraphs,
		FExecuteAction::CreateLambda([](){bool& bEnableGraphsVisualization = ULogVisualizerSessionSettings::StaticClass()->GetDefaultObject<ULogVisualizerSessionSettings>()->bEnableGraphsVisualization; bEnableGraphsVisualization = !bEnableGraphsVisualization; }),
		FCanExecuteAction::CreateLambda([this]()->bool{return bGotHistogramData; }),
		FIsActionChecked::CreateLambda([]()->bool{return ULogVisualizerSessionSettings::StaticClass()->GetDefaultObject<ULogVisualizerSessionSettings>()->bEnableGraphsVisualization; }),
		FIsActionButtonVisible::CreateLambda([this]()->bool{return bGotHistogramData; }));
	ActionList.MapAction(Commands.ResetData, 
		FExecuteAction::CreateRaw(this, &SVisualLogger::ResetData),
		FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute),
		FIsActionChecked(), 
		FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute));

	ActionList.MapAction(
		Commands.MoveCursorLeft,
		FExecuteAction::CreateRaw(this, &SVisualLogger::OnMoveCursorLeftCommand));
	ActionList.MapAction(
		Commands.MoveCursorRight,
		FExecuteAction::CreateRaw(this, &SVisualLogger::OnMoveCursorRightCommand));


	// Tab Spawners
	TabManager = FGlobalTabmanager::Get()->NewTabManager(ConstructUnderMajorTab);
	TSharedRef<FWorkspaceItem> AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("VisualLoggerGroupName", "Visual Logger"));

	TabManager->RegisterTabSpawner(ToolbarTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, ToolbarTabId))
		.SetDisplayName(LOCTEXT("ToolbarTabTitle", "Toolbar"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "ToolbarTabIcon"));

	TabManager->RegisterTabSpawner(FiltersTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, FiltersTabId))
		.SetDisplayName(LOCTEXT("FiltersTabTitle", "Filters"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "FiltersTabIcon"));

	TabManager->RegisterTabSpawner(MainViewTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, MainViewTabId))
		.SetDisplayName(LOCTEXT("MainViewTabTitle", "MainView"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "MainViewTabIcon"));

	TabManager->RegisterTabSpawner(LogsListTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, LogsListTabId))
		.SetDisplayName(LOCTEXT("LogsListTabTitle", "LogsList"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "LogsListTabIcon"));

	TabManager->RegisterTabSpawner(StatusViewTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, StatusViewTabId))
		.SetDisplayName(LOCTEXT("StatusViewTabTitle", "StatusView"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "StatusViewTabIcon"));

	// Default Layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("VisualLoggerLayout_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(ToolbarTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(FiltersTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(MainViewTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.6f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(StatusViewTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
					->SetSizeCoefficient(0.3f)
					)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(LogsListTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
					->SetSizeCoefficient(0.7f)
					)
			)
		);
	TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &SVisualLogger::HandleTabManagerPersistLayout));

	// Load the potentially previously saved new layout
	TSharedRef<FTabManager::FLayout> UserConfiguredNewLayout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout);

	TabManager->RestoreFrom(UserConfiguredNewLayout, TSharedPtr<SWindow>());

	// Window Menu
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());
#if 0 //disabled File menu for now (SebaK)
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("FileMenuLabel", "File"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateRaw(this, &SVisualLogger::FillFileMenu, TabManager),
		"File"
		);
#endif
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateStatic(&SVisualLogger::FillWindowMenu, TabManager),
		"Window"
		);

	MenuBarBuilder.AddMenuEntry(
		LOCTEXT("SettingsMenuLabel", "Settings"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(
			[this](){
				ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
				if (SettingsModule != nullptr)
				{
					SettingsModule->ShowViewer("Editor", "General", "VisualLogger");
				}
			}
		)),
		"Settings"
		);


	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MenuBarBuilder.MakeWidget()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				TabManager->RestoreFrom(Layout, ConstructUnderWindow).ToSharedRef()
			]
		];

	VisualLoggerCanvasRenderer = MakeShareable(new FVisualLoggerCanvasRenderer());

	DrawOnCanvasDelegateHandle = UDebugDrawService::Register(TEXT("VisLog"), FDebugDrawDelegate::CreateRaw(VisualLoggerCanvasRenderer.Get(), &FVisualLoggerCanvasRenderer::DrawOnCanvas));
	//UGameplayDebuggingComponent::OnDebuggingTargetChangedDelegate.AddSP(this, &SVisualLogger::SelectionChanged);
}

FReply SVisualLogger::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FUICommandList& ActionList = *CommandList;
	return ActionList.ProcessCommandBindings(InKeyEvent) ? FReply::Handled() : FReply::Unhandled();
}

void SVisualLogger::HandleMajorTabPersistVisualState()
{
	// save any settings here
}

void SVisualLogger::HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
}

void SVisualLogger::FillFileMenu(FMenuBuilder& MenuBuilder, const TSharedPtr<FTabManager> InTabManager)
{
	MenuBuilder.BeginSection("LogFile", LOCTEXT("FileMenu", "Log File"));
	{
		MenuBuilder.AddMenuEntry(FVisualLoggerCommands::Get().LoadFromVLog);
		MenuBuilder.AddMenuEntry(FVisualLoggerCommands::Get().SaveToVLog);
	}
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("LogFilters", LOCTEXT("FIlterMenu", "Log Filters"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("LoadPreset", "Load Preset"), LOCTEXT("LoadPresetTooltip", "Load filter's preset"),
			FNewMenuDelegate::CreateRaw(this, &SVisualLogger::FillLoadPresetMenu));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SavePreset", "Save Preset"), LOCTEXT("SavePresetTooltip", "Save filter's setting as preset"),
			FSlateIcon(), FUIAction(
			FExecuteAction::CreateLambda(
				[this](){
				}
			)));
	}
	MenuBuilder.EndSection();
}

void SVisualLogger::FillLoadPresetMenu(FMenuBuilder& MenuBuilder)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	
	MenuBuilder.BeginSection("FilterPresets", LOCTEXT("FilterPresets", "Presets"));
#if 0 //disabled for now, we don't use presets yet
	for (auto& CurrentPreset : Settings->FilterPresets)
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(CurrentPreset.FilterName), LOCTEXT("LoadPresetTooltip", "Load filter's preset"),
			FSlateIcon(), FUIAction(
			FExecuteAction::CreateLambda([this, CurrentPreset](){ this->SetFiltersPreset(CurrentPreset); }
			))
		);
	}
#endif
	MenuBuilder.EndSection();
}

void SVisualLogger::SetFiltersPreset(const struct FFiltersPreset& Preset)
{

}

void SVisualLogger::FillWindowMenu(FMenuBuilder& MenuBuilder, const TSharedPtr<FTabManager> TabManager)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

TSharedRef<SDockTab> SVisualLogger::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier) const
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;
	bool AutoSizeTab = false;

	if (TabIdentifier == ToolbarTabId)
	{
		TabWidget = SNew(SVisualLoggerToolbar, CommandList);
		AutoSizeTab = true;
	}
	else if (TabIdentifier == FiltersTabId)
	{
		TabWidget = SAssignNew(VisualLoggerFilters, SVisualLoggerFilters, CommandList);
		AutoSizeTab = true;
	}
	else if (TabIdentifier == MainViewTabId)
	{
		TabWidget = SAssignNew(MainView, SVisualLoggerView, CommandList).OnFiltersSearchChanged(this, &SVisualLogger::OnFiltersSearchChanged);
		AutoSizeTab = false;
	}
	else if (TabIdentifier == LogsListTabId)
	{
		TabWidget = SAssignNew(LogsList, SVisualLoggerLogsList, CommandList);
		AutoSizeTab = false;
	}
	else if (TabIdentifier == StatusViewTabId)
	{
		TabWidget = SAssignNew(StatusView, SVisualLoggerStatusView, CommandList);
		AutoSizeTab = false;
	}
	
	return SNew(SDockTab)
		.ShouldAutosize(AutoSizeTab)
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}

bool SVisualLogger::HandleStartRecordingCommandCanExecute() const
{
	return !FVisualLogger::Get().IsRecording();
}


void SVisualLogger::HandleStartRecordingCommandExecute()
{
	FVisualLogger::Get().SetIsRecording(true);
}


bool SVisualLogger::HandleStartRecordingCommandIsVisible() const
{
	return !FVisualLogger::Get().IsRecording();
}

bool SVisualLogger::HandleStopRecordingCommandCanExecute() const
{
	return FVisualLogger::Get().IsRecording();
}


void SVisualLogger::HandleStopRecordingCommandExecute()
{
	FVisualLogger::Get().SetIsRecording(false);

	UWorld* World = FLogVisualizer::Get().GetWorld();
	if (AVisualLoggerCameraController::IsEnabled(World))
	{
		AVisualLoggerCameraController::DisableCamera(World);
	}
}


bool SVisualLogger::HandleStopRecordingCommandIsVisible() const
{
	return FVisualLogger::Get().IsRecording();
}

bool SVisualLogger::HandlePauseCommandCanExecute() const
{
	return !bPausedLogger;
}

void SVisualLogger::HandlePauseCommandExecute()
{
	if (ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bUsePlayersOnlyForPause)
	{
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if (Context.World() != nullptr)
			{
				Context.World()->bPlayersOnlyPending = true;
			}
		}
	}

	bPausedLogger = true;
}

bool SVisualLogger::HandlePauseCommandIsVisible() const
{
	return HandlePauseCommandCanExecute();
}

bool SVisualLogger::HandleResumeCommandCanExecute() const
{
	return bPausedLogger;
}

void SVisualLogger::HandleResumeCommandExecute()
{
	if (ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bUsePlayersOnlyForPause)
	{
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if (Context.World() != nullptr)
			{
				Context.World()->bPlayersOnly = false;
				Context.World()->bPlayersOnlyPending = false;
			}
		}
	}

	bPausedLogger = false;
	for (const auto& CurrentEntry : OnPauseCacheForEntries)
	{
		OnNewLogEntry(CurrentEntry);
	}
	OnPauseCacheForEntries.Reset();

}

bool SVisualLogger::HandleResumeCommandIsVisible() const
{
	return HandleResumeCommandCanExecute();
}

bool SVisualLogger::HandleCameraCommandIsChecked() const
{
	UWorld* World = FLogVisualizer::Get().GetWorld();
	return World && AVisualLoggerCameraController::IsEnabled(World);
}

bool SVisualLogger::HandleCameraCommandCanExecute() const
{
	UWorld* World = FLogVisualizer::Get().GetWorld();
	return FVisualLogger::Get().IsRecording() && World && (World->bPlayersOnly || World->bPlayersOnlyPending) && World->IsPlayInEditor() && (GEditor && !GEditor->bIsSimulatingInEditor);
}

void SVisualLogger::HandleCameraCommandExecute()
{
	UWorld* World = FLogVisualizer::Get().GetWorld();
	if (AVisualLoggerCameraController::IsEnabled(World))
	{
		AVisualLoggerCameraController::DisableCamera(World);
	}
	else
	{
		// switch debug cam on
		CameraController = AVisualLoggerCameraController::EnableCamera(World);
		if (CameraController.IsValid())
		{
			//CameraController->OnActorSelected = ALogVisualizerCameraController::FActorSelectedDelegate::CreateSP(this, &SVisualLogger::CameraActorSelected);
			//CameraController->OnIterateLogEntries = ALogVisualizerCameraController::FLogEntryIterationDelegate::CreateSP(this, &SVisualLogger::IncrementCurrentLogIndex);
		}
	}
}

bool SVisualLogger::HandleLoadCommandCanExecute() const
{
	return true;
}

void SVisualLogger::HandleLoadCommandExecute()
{
	FArchive Ar;
	TArray<FVisualLogDevice::FVisualLogEntryItem> RecordedLogs;

	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		const FString DefaultBrowsePath = FString::Printf(TEXT("%slogs/"), *FPaths::GameSavedDir());

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("OpenProjectBrowseTitle", "Open Project").ToString(),
			DefaultBrowsePath,
			TEXT(""),
			LogVisualizer::LoadFileTypes,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}

	if (bOpened && OpenFilenames.Num() > 0)
	{
		OnNewWorld(nullptr);
		for (int FilenameIndex = 0; FilenameIndex < OpenFilenames.Num(); ++FilenameIndex)
		{
			FString CurrentFileName = OpenFilenames[FilenameIndex];
			const bool bIsBinaryFile = CurrentFileName.Find(TEXT(".bvlog")) != INDEX_NONE;
			if (bIsBinaryFile)
			{
				FArchive* FileAr = IFileManager::Get().CreateFileReader(*CurrentFileName);
				FVisualLoggerHelpers::Serialize(*FileAr, RecordedLogs);
				FileAr->Close();
				delete FileAr;
				FileAr = NULL;

				for (FVisualLogDevice::FVisualLogEntryItem& CurrentItem : RecordedLogs)
				{
					OnNewLogEntry(CurrentItem);
				}
			}
		}
	}
}

bool SVisualLogger::HandleSaveCommandCanExecute() const
{ 
	TArray<TSharedPtr<class STimeline> > OutTimelines;
	MainView->GetTimelines(OutTimelines);
	return OutTimelines.Num() > 0;
}

void SVisualLogger::HandleSaveCommandExecute()
{
	TArray<TSharedPtr<class STimeline> > OutTimelines;
	MainView->GetTimelines(OutTimelines, true);
	if (OutTimelines.Num() == 0)
	{
		MainView->GetTimelines(OutTimelines);
	}

	if (OutTimelines.Num())
	{
		// Prompt the user for the filenames
		TArray<FString> SaveFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bSaved = false;
		if (DesktopPlatform)
		{
			void* ParentWindowWindowHandle = NULL;

			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
			if (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
			{
				ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
			}

			const FString DefaultBrowsePath = FString::Printf(TEXT("%slogs/"), *FPaths::GameSavedDir());
			bSaved = DesktopPlatform->SaveFileDialog(
				ParentWindowWindowHandle,
				LOCTEXT("NewProjectBrowseTitle", "Choose a project location").ToString(),
				DefaultBrowsePath,
				TEXT(""),
				LogVisualizer::SaveFileTypes,
				EFileDialogFlags::None,
				SaveFilenames
				);
		}

		if (bSaved)
		{
			if (SaveFilenames.Num() > 0)
			{
				TArray<FVisualLogDevice::FVisualLogEntryItem> FrameCache;
				for (auto CurrentItem : OutTimelines)
				{
					FrameCache.Append(CurrentItem->GetEntries());
				}

				if (FrameCache.Num())
				{
					FArchive* FileArchive = IFileManager::Get().CreateFileWriter(*SaveFilenames[0]);
					FVisualLoggerHelpers::Serialize(*FileArchive, FrameCache);
					FileArchive->Close();
					delete FileArchive;
					FileArchive = NULL;
				}
			}
		}
	}
}

void SVisualLogger::ResetData()
{
	if (MainView.IsValid())
	{
		MainView->ResetData();
	}

	if (VisualLoggerFilters.IsValid())
	{
		VisualLoggerFilters->ResetData();
	}

	if (LogsList.IsValid())
	{
		LogsList->OnItemSelectionChanged(FVisualLogDevice::FVisualLogEntryItem());
	}

	if (StatusView.IsValid())
	{
		StatusView->OnItemSelectionChanged(FVisualLogDevice::FVisualLogEntryItem());
	}

	if (VisualLoggerCanvasRenderer.IsValid())
	{
		VisualLoggerCanvasRenderer->OnItemSelectionChanged(FVisualLogEntry());
		VisualLoggerCanvasRenderer->ObjectSelectionChanged(NULL);
	}

	AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(FLogVisualizer::Get().GetVisualLoggerHelperActor());
	if (HelperActor)
	{
		HelperActor->OnItemSelectionChanged(FVisualLogDevice::FVisualLogEntryItem());
		HelperActor->ObjectSelectionChanged(NULL);
	}

	bGotHistogramData = false;
	OnPauseCacheForEntries.Reset();
}

void SVisualLogger::OnNewWorld(UWorld* NewWorld)
{
	InternalDevice->SerLastWorld(NewWorld);
	if (ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bResetDataWithNewSession)
	{
		ResetData();
	}
}

void SVisualLogger::OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry)
{
	if (!bPausedLogger)
	{
		CollectNewCategories(Entry);
		MainView->OnNewLogEntry(Entry);
		bGotHistogramData = bGotHistogramData || Entry.Entry.HistogramSamples.Num() > 0;
	}
	else
	{
		OnPauseCacheForEntries.Add(Entry);
	}
}

void SVisualLogger::CollectNewCategories(const FVisualLogDevice::FVisualLogEntryItem& Item)
{
	TArray<FVisualLoggerCategoryVerbosityPair> Categories;
	FVisualLoggerHelpers::GetCategories(Item.Entry, Categories);
	for (auto& CategoryAndVerbosity : Categories)
	{
		VisualLoggerFilters->AddFilter(CategoryAndVerbosity.CategoryName.ToString());
	}

	TMap<FString, TArray<FString> > OutCategories;
	FVisualLoggerHelpers::GetHistogramCategories(Item.Entry, OutCategories);
	for (const auto& CurrentCategory : OutCategories)
	{
		for (const auto& CurrentDataName : CurrentCategory.Value)
		{
			VisualLoggerFilters->AddFilter(CurrentCategory.Key, CurrentDataName);
		}
	}
}

void SVisualLogger::OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem& EntryItem)
{
	LogsList->OnItemSelectionChanged(EntryItem);
	StatusView->OnItemSelectionChanged(EntryItem);
	VisualLoggerCanvasRenderer->OnItemSelectionChanged(EntryItem.Entry);
	VisualLoggerFilters->OnItemSelectionChanged(EntryItem.Entry);
	AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(FLogVisualizer::Get().GetVisualLoggerHelperActor());
	if (HelperActor)
	{
		HelperActor->OnItemSelectionChanged(EntryItem);
	}
}

void SVisualLogger::OnFiltersChanged()
{
	LogsList->OnFiltersChanged();
	MainView->OnFiltersChanged();
}

void SVisualLogger::OnObjectSelectionChanged(TSharedPtr<class STimeline> TimeLine)
{
	VisualLoggerCanvasRenderer->ObjectSelectionChanged(TimeLine);
	AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(FLogVisualizer::Get().GetVisualLoggerHelperActor());
	if (HelperActor)
	{
		HelperActor->ObjectSelectionChanged(TimeLine);
	}
	MainView->OnObjectSelectionChanged(TimeLine);
	FLogVisualizer::Get().OnObjectSelectionChanged(TimeLine);
}

void SVisualLogger::OnFiltersSearchChanged(const FText& Filter)
{
	FCategoryFiltersManager::Get().SetSearchString(Filter.ToString());

	if (VisualLoggerFilters.IsValid())
	{
		VisualLoggerFilters->OnFiltersSearchChanged(Filter);
	}
	if (LogsList.IsValid())
	{
		LogsList->OnFiltersSearchChanged(Filter);
	}
	if (MainView.IsValid())
	{
		MainView->OnFiltersSearchChanged(Filter);
	}
}

void SVisualLogger::OnLogLineSelectionChanged(TSharedPtr<struct FLogEntryItem> SelectedItem, int64 UserData, FName TagName)
{
	TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto Iterator = AllExtensions.CreateIterator(); Iterator; ++Iterator)
	{
		FVisualLogExtensionInterface* Extension = (*Iterator).Value;
		if (Extension != NULL)
		{
			Extension->LogEntryLineSelectionChanged(SelectedItem, UserData, TagName);
		}
	}

	AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(FLogVisualizer::Get().GetVisualLoggerHelperActor());
	if (HelperActor)
	{
		HelperActor->OnItemSelectionChanged(LogsList->GetCurrentLogEntry());
		HelperActor->MarkComponentsRenderStateDirty();
	}
}

void SVisualLogger::OnMoveCursorLeftCommand()
{
	FLogVisualizer::Get().GotoNextItem();
}

void SVisualLogger::OnMoveCursorRightCommand()
{
	FLogVisualizer::Get().GotoPreviousItem();
}

void SVisualLogger::GetTimelines(TArray<TSharedPtr<class STimeline> >& TimeLines, bool bOnlySelectedOnes)
{
	if (MainView.IsValid())
	{
		MainView->GetTimelines(TimeLines, bOnlySelectedOnes);
	}
}

#undef LOCTEXT_NAMESPACE
