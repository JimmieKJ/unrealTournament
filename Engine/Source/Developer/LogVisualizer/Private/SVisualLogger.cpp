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
	static const FString LoadFileTypes = FString::Printf(TEXT("%s (*.vlog;*.%s)|*.vlog;*.%s"), *LogFileDescription, VISLOG_FILENAME_EXT, VISLOG_FILENAME_EXT);
	static const FString SaveFileTypes = FString::Printf(TEXT("%s (*.%s)|*.%s"), *LogFileDescription, VISLOG_FILENAME_EXT, VISLOG_FILENAME_EXT);
}

struct FVisualLoggerInterface : public IVisualLoggerInterface
{
	FVisualLoggerInterface(TSharedPtr<SVisualLogger> InVisualLogger, FVisualLoggerEvents InVisualLoggerEvents)
		: VisualLogger(InVisualLogger)
	{
		VisualLoggerEvents = InVisualLoggerEvents;
	}

	virtual ~FVisualLoggerInterface() {}

	virtual bool HasValidCategories(TArray<FVisualLoggerCategoryVerbosityPair> Categories) override
	{
		for (const auto& CurrentCategory : Categories)
		{
			if (VisualLogger->GetVisualLoggerFilters()->IsFilterEnabled(CurrentCategory.CategoryName.ToString(), CurrentCategory.Verbosity))
			{
				return true;
			}
		}
		return false;
	}

	bool IsValidCategory(const FString& InCategoryName, TEnumAsByte<ELogVerbosity::Type> Verbosity)
	{
		return VisualLogger->GetVisualLoggerFilters()->IsFilterEnabled(InCategoryName, Verbosity);
	}

	bool IsValidCategory(const FString& InGraphName, const FString& InDataName, TEnumAsByte<ELogVerbosity::Type> Verbosity = ELogVerbosity::All)
	{
		return VisualLogger->GetVisualLoggerFilters()->IsFilterEnabled(InGraphName, InDataName, Verbosity);
	}

	virtual FLinearColor GetCategoryColor(FName Category) override
	{
		return VisualLogger->GetVisualLoggerFilters()->GetColorForUsedCategory(Category.ToString());
	}

	UWorld* GetWorld() const
	{
		UWorld* World = NULL;
		UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
		if (GIsEditor && EEngine != NULL)
		{
			// lets use PlayWorld during PIE/Simulate and regular world from editor otherwise, to draw debug information
			World = EEngine->PlayWorld != NULL ? EEngine->PlayWorld : EEngine->GetEditorWorldContext().World();

		}
		else if (!GIsEditor)
		{

			World = GEngine->GetWorld();
		}

		if (World == NULL)
		{
			World = GWorld;
		}

		return World;
	}

	class AActor* GetVisualLoggerHelperActor() override
	{
		UWorld* World = GetWorld();
		for (TActorIterator<AVisualLoggerRenderingActor> It(World); It; ++It)
		{
			return *It;
		}

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.bNoCollisionFail = true;
		SpawnInfo.Name = *FString::Printf(TEXT("VisualLoggerRenderingActor"));
		AActor* HelperActor = World->SpawnActor<AVisualLoggerRenderingActor>(SpawnInfo);

		return HelperActor;
	}

protected:
	TSharedPtr<SVisualLogger> VisualLogger;
};

SVisualLogger::FVisualLoggerDevice::FVisualLoggerDevice(SVisualLogger* InOwner)
	:Owner(InOwner)
{
}

SVisualLogger::FVisualLoggerDevice::~FVisualLoggerDevice() 
{
}

void SVisualLogger::FVisualLoggerDevice::Serialize(const class UObject* LogOwner, FName OwnerName, const FVisualLogEntry& LogEntry)
{
	if (Owner == NULL)
	{
		return;
	}

	Owner->OnNewLogEntry(FVisualLogDevice::FVisualLogEntryItem(OwnerName, LogEntry));
}

/* SMessagingDebugger constructors
*****************************************************************************/

SVisualLogger::SVisualLogger() : SCompoundWidget(), CommandList(MakeShareable(new FUICommandList))
{ 
	InternalDevice = MakeShareable(new FVisualLoggerDevice(this));
	FVisualLogger::Get().AddDevice(InternalDevice.Get());
}

SVisualLogger::~SVisualLogger()
{

}

void SVisualLogger::OnTabLosed()
{
	UDebugDrawService::Unregister(FDebugDrawDelegate::CreateRaw(VisualLoggerCanvasRenderer.Get(), &FVisualLoggerCanvasRenderer::DrawOnCanvas));
	VisualLoggerCanvasRenderer = NULL;

	FVisualLogger::Get().RemoveDevice(InternalDevice.Get());
	InternalDevice = NULL;
}

/* SMessagingDebugger interface
*****************************************************************************/

void SVisualLogger::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	//////////////////////////////////////////////////////////////////////////
	// Visual Logger Events
	FVisualLoggerEvents VisualLoggerEvents;
	VisualLoggerEvents.OnItemSelectionChanged = FOnItemSelectionChanged::CreateRaw(this, &SVisualLogger::OnItemSelectionChanged);
	VisualLoggerEvents.OnFiltersChanged = FOnFiltersChanged::CreateRaw(this, &SVisualLogger::OnFiltersChanged);
	VisualLoggerEvents.OnObjectSelectionChanged = FOnObjectSelectionChanged::CreateRaw(this, &SVisualLogger::OnObjectSelectionChanged);

	//////////////////////////////////////////////////////////////////////////
	// Visual Logger Interface
	VisualLoggerInterface = MakeShareable(new FVisualLoggerInterface(SharedThis(this), VisualLoggerEvents));

	//////////////////////////////////////////////////////////////////////////
	// Command Action Lists
	const FVisualLoggerCommands& Commands = FVisualLoggerCommands::Get();
	FUICommandList& ActionList = *CommandList;

	ActionList.MapAction(Commands.StartRecording, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandIsVisible));
	ActionList.MapAction(Commands.StopRecording, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandIsVisible));
	ActionList.MapAction(Commands.Pause, FExecuteAction::CreateRaw(this, &SVisualLogger::HandlePauseCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandlePauseCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandlePauseCommandIsVisible));
	ActionList.MapAction(Commands.Resume, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleResumeCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleResumeCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleResumeCommandIsVisible));
	ActionList.MapAction(Commands.Load, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleLoadCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleLoadCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleLoadCommandCanExecute));
	ActionList.MapAction(Commands.Save, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute));
	ActionList.MapAction(Commands.FreeCamera, 
		FExecuteAction::CreateRaw(this, &SVisualLogger::HandleCameraCommandExecute), 
		FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleCameraCommandCanExecute), 
		FIsActionChecked::CreateRaw(this, &SVisualLogger::HandleCameraCommandIsChecked),
		FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleCameraCommandCanExecute));
	ActionList.MapAction(Commands.ToggleGraphs,
		FExecuteAction::CreateLambda([](){bool& bEnableGraphsVisualization = ULogVisualizerSessionSettings::StaticClass()->GetDefaultObject<ULogVisualizerSessionSettings>()->bEnableGraphsVisualization; bEnableGraphsVisualization = !bEnableGraphsVisualization; }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()->bool{return ULogVisualizerSessionSettings::StaticClass()->GetDefaultObject<ULogVisualizerSessionSettings>()->bEnableGraphsVisualization; }),
		FIsActionButtonVisible());


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

	VisualLoggerCanvasRenderer = MakeShareable(new FVisualLoggerCanvasRenderer(VisualLoggerInterface));

	UDebugDrawService::Register(TEXT("VisLog"), FDebugDrawDelegate::CreateRaw(VisualLoggerCanvasRenderer.Get(), &FVisualLoggerCanvasRenderer::DrawOnCanvas));
	//UGameplayDebuggingComponent::OnDebuggingTargetChangedDelegate.AddSP(this, &SVisualLogger::SelectionChanged);
}

void SVisualLogger::HandleMajorTabPersistVisualState()
{
	// save any settings here
}

void SVisualLogger::HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
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
		TabWidget = SAssignNew(VisualLoggerFilters, SVisualLoggerFilters, CommandList, VisualLoggerInterface);
		AutoSizeTab = true;
	}
	else if (TabIdentifier == MainViewTabId)
	{
		TabWidget = SAssignNew(MainView, SVisualLoggerView, CommandList, VisualLoggerInterface).OnFiltersSearchChanged(this, &SVisualLogger::OnFiltersSearchChanged);
		AutoSizeTab = false;
	}
	else if (TabIdentifier == LogsListTabId)
	{
		TabWidget = SAssignNew(LogsList, SVisualLoggerLogsList, CommandList, VisualLoggerInterface);
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

	UWorld* World = VisualLoggerInterface->GetWorld();
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
	UWorld* World = VisualLoggerInterface->GetWorld();
	return FVisualLogger::Get().IsRecording() && World && !World->bPlayersOnly && !World->bPlayersOnlyPending;
}

void SVisualLogger::HandlePauseCommandExecute()
{
	UWorld* World = VisualLoggerInterface->GetWorld();
	if (World != NULL)
	{
		World->bPlayersOnlyPending = true;
	}
}

bool SVisualLogger::HandlePauseCommandIsVisible() const
{
	return HandlePauseCommandCanExecute();
}

bool SVisualLogger::HandleResumeCommandCanExecute() const
{
	UWorld* World = VisualLoggerInterface->GetWorld();
	return FVisualLogger::Get().IsRecording() && World && (World->bPlayersOnly || World->bPlayersOnlyPending);
}

void SVisualLogger::HandleResumeCommandExecute()
{
	UWorld* World = VisualLoggerInterface->GetWorld();
	if (World != NULL)
	{
		World->bPlayersOnly = false;
		World->bPlayersOnlyPending = false;
	}
}

bool SVisualLogger::HandleResumeCommandIsVisible() const
{
	return HandleResumeCommandCanExecute();
}

bool SVisualLogger::HandleCameraCommandIsChecked() const
{
	UWorld* World = VisualLoggerInterface->GetWorld();
	return World && AVisualLoggerCameraController::IsEnabled(World);
}

bool SVisualLogger::HandleCameraCommandCanExecute() const
{
	UWorld* World = VisualLoggerInterface->GetWorld();
	return FVisualLogger::Get().IsRecording() && World && (World->bPlayersOnly || World->bPlayersOnlyPending) && World->IsPlayInEditor() && (GEditor && !GEditor->bIsSimulatingInEditor);
}

void SVisualLogger::HandleCameraCommandExecute()
{
	UWorld* World = VisualLoggerInterface->GetWorld();
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

void SVisualLogger::OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry)
{
	CollectNewCategories(Entry);
	MainView->OnNewLogEntry(Entry);
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
	AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(VisualLoggerInterface->GetVisualLoggerHelperActor());
	if (HelperActor)
	{
		HelperActor->OnItemSelectionChanged(EntryItem, VisualLoggerInterface);
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
	AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(VisualLoggerInterface->GetVisualLoggerHelperActor());
	if (HelperActor)
	{
		HelperActor->ObjectSelectionChanged(TimeLine);
	}
	MainView->OnObjectSelectionChanged(TimeLine);
}

void SVisualLogger::OnFiltersSearchChanged(const FText& Filter)
{
	VisualLoggerFilters->OnFiltersSearchChanged(Filter);
	LogsList->OnFiltersSearchChanged(Filter);
	MainView->OnFiltersSearchChanged(Filter);
}
#undef LOCTEXT_NAMESPACE
