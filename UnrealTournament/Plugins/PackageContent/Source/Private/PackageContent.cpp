// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PackageContentPrivatePCH.h"
#include "PackageContent.h"
#include "LevelEditor.h"
#include "PackageContentCommands.h"
#include "PackageContentStyle.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "EdGraphSchema_K2.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "SHyperlink.h"
#include "UTUnrealEdEngine.h"
#include "ITargetPlatformManagerModule.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Net/UnrealNetwork.h"
#include "Misc/NetworkVersion.h"

#define LOCTEXT_NAMESPACE "PackageContent"

DEFINE_LOG_CATEGORY(LogPackageContent);


class SPackageCompleteChoiceDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPackageCompleteChoiceDialog)	{}
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(FText, Message)
		SLATE_ATTRIBUTE(float, WrapMessageAt)
		SLATE_ATTRIBUTE(EAppMsgType::Type, MessageType)
		SLATE_ATTRIBUTE(FString, FilePath)
		SLATE_ATTRIBUTE(FText, FilePathLinkText)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		ParentWindow = InArgs._ParentWindow.Get();
		ParentWindow->SetWidgetToFocusOnActivate(SharedThis(this));
		Response = EAppReturnType::Cancel;

		FSlateFontInfo MessageFont(FEditorStyle::GetFontStyle("StandardDialog.LargeFont"));
		MyMessage = InArgs._Message;
		FilePath = InArgs._FilePath.Get();
		FText FilePathLinkText = InArgs._FilePathLinkText.Get();

		TSharedPtr<SUniformGridPanel> ButtonBox;

		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillHeight(1.0f)
				.MaxHeight(550)
				.Padding(12.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(STextBlock)
						.Text(MyMessage)
						.Font(MessageFont)
						.WrapTextAt(InArgs._WrapMessageAt)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Bottom)
					.Padding(12.0f)
					[
						SNew(SHyperlink)
						.OnNavigate(this, &SPackageCompleteChoiceDialog::HandleFilePathHyperlinkNavigate)
						.Text(FilePathLinkText)
						.ToolTipText(FText::FromString(FilePath))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(2.f)
					[
						SAssignNew(ButtonBox, SUniformGridPanel)
						.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
						.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
						.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					]
				]
			]
		];

		int32 SlotIndex = 0;

#define ADD_SLOT(Button)\
	ButtonBox->AddSlot(SlotIndex++, 0)\
	[\
	SNew(SButton)\
	.Text(EAppReturnTypeToText(EAppReturnType::Button))\
	.OnClicked(this, &SPackageCompleteChoiceDialog::HandleButtonClicked, EAppReturnType::Button)\
	.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))\
	.HAlign(HAlign_Center)\
	];

		switch (InArgs._MessageType.Get())
		{
		case EAppMsgType::Ok:
			ADD_SLOT(Ok)
				break;
		case EAppMsgType::YesNo:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
				break;
		case EAppMsgType::OkCancel:
			ADD_SLOT(Ok)
			ADD_SLOT(Cancel)
				break;
		case EAppMsgType::YesNoCancel:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(Cancel)
				break;
		case EAppMsgType::CancelRetryContinue:
			ADD_SLOT(Cancel)
			ADD_SLOT(Retry)
			ADD_SLOT(Continue)
				break;
		case EAppMsgType::YesNoYesAllNoAll:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			ADD_SLOT(NoAll)
				break;
		case EAppMsgType::YesNoYesAllNoAllCancel:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
			ADD_SLOT(NoAll)
			ADD_SLOT(Cancel)
				break;
		case EAppMsgType::YesNoYesAll:
			ADD_SLOT(Yes)
			ADD_SLOT(No)
			ADD_SLOT(YesAll)
				break;
		default:
			UE_LOG(LogPackageContent, Fatal, TEXT("Invalid Message Type"));
		}

#undef ADD_SLOT
	
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION


	EAppReturnType::Type GetResponse()
	{
		return Response;
	}
	
	virtual	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		//see if we pressed the Enter or Spacebar keys
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return HandleButtonClicked(EAppReturnType::Cancel);
		}

		if (InKeyEvent.GetKey() == EKeys::C && InKeyEvent.IsControlDown())
		{
			CopyMessageToClipboard();

			return FReply::Handled();
		}

		//if it was some other button, ignore it
		return FReply::Unhandled();
	}

	/** Override the base method to allow for keyboard focus */
	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	/** Converts an EAppReturnType into a localized FText */
	static FText EAppReturnTypeToText(EAppReturnType::Type ReturnType)
	{
		switch (ReturnType)
		{
		case EAppReturnType::No:
			return LOCTEXT("EAppReturnTypeNo", "No");
		case EAppReturnType::Yes:
			return LOCTEXT("EAppReturnTypeYes", "Yes");
		case EAppReturnType::YesAll:
			return LOCTEXT("EAppReturnTypeYesAll", "Yes All");
		case EAppReturnType::NoAll:
			return LOCTEXT("EAppReturnTypeNoAll", "No All");
		case EAppReturnType::Cancel:
			return LOCTEXT("EAppReturnTypeCancel", "Cancel");
		case EAppReturnType::Ok:
			return LOCTEXT("EAppReturnTypeOk", "OK");
		case EAppReturnType::Retry:
			return LOCTEXT("EAppReturnTypeRetry", "Retry");
		case EAppReturnType::Continue:
			return LOCTEXT("EAppReturnTypeContinue", "Continue");
		default:
			return LOCTEXT("MissingType", "MISSING RETURN TYPE");
		}
	}

protected:

	/**
	 * Copies the message text to the clipboard.
	 */
	void CopyMessageToClipboard()
	{
		FPlatformMisc::ClipboardCopy(*MyMessage.Get().ToString());
	}


private:

	// Handles clicking a message box button.
	FReply HandleButtonClicked(EAppReturnType::Type InResponse)
	{
		Response = InResponse;

		ResultCallback.ExecuteIfBound(ParentWindow.ToSharedRef(), Response);


		ParentWindow->RequestDestroyWindow();

		return FReply::Handled();
	}

	// Handles clicking the 'Copy Message' hyper link.
	void HandleCopyMessageHyperlinkNavigate()
	{
		CopyMessageToClipboard();
	}
	
	void HandleFilePathHyperlinkNavigate()
	{
		FPlatformProcess::ExploreFolder(*FilePath);
	}

public:
	/** Callback delegate that is triggered, when the dialog is run in non-modal mode */
	FOnMsgDlgResult ResultCallback;

private:

	EAppReturnType::Type Response;
	TSharedPtr<SWindow> ParentWindow;
	TAttribute<FText> MyMessage;
	FString FilePath;
};

const FVector2D FPackageContent::DEFAULT_WINDOW_SIZE = FVector2D(600, 400);

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
		ActionList->MapAction(Commands.PackageTaunt, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageTauntWindow));
		ActionList->MapAction(Commands.PackageMutator, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageMutatorWindow));
		ActionList->MapAction(Commands.PackageCharacter, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageCharacterWindow));
		ActionList->MapAction(Commands.PackageCrosshair, FExecuteAction::CreateSP(this, &FPackageContent::OpenPackageCrosshairWindow));
	}
}

void FPackageContent::OpenPackageLevelWindow()
{
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = true;
	bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		UE_LOG(LogPackageContent, Log, TEXT("Starting to publish this level!"));
		
		FString MapName = GWorld->GetMapName();
		PackageDLC(MapName, LOCTEXT("PackageWeaponTaskName", "Packaging Level"), LOCTEXT("CookingTaskName", "Publishing"));
	}
}

class FPackageContentNotificationTask
{
public:

	FPackageContentNotificationTask(TWeakPtr<SNotificationItem> InNotificationItemPtr, SNotificationItem::ECompletionState InCompletionState, const FText& InText)
		: CompletionState(InCompletionState)
		, NotificationItemPtr(InNotificationItemPtr)
		, Text(InText)
	{ }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (NotificationItemPtr.IsValid())
		{
			if (CompletionState == SNotificationItem::CS_Fail)
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
			}
			else
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
			}

			TSharedPtr<SNotificationItem> NotificationItem = NotificationItemPtr.Pin();
			NotificationItem->SetText(Text);
			NotificationItem->SetCompletionState(CompletionState);
			NotificationItem->ExpireAndFadeout();
		}
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPackageContentNotificationTask, STATGROUP_TaskGraphTasks);
	}

private:

	SNotificationItem::ECompletionState CompletionState;
	TWeakPtr<SNotificationItem> NotificationItemPtr;
	FText Text;
};

class FPackageContentInEditorPart2Task
{
public:
	FPackageContentInEditorPart2Task(const FString& InDLCName, FPackageContent* InPackageContent)
		: DLCName(InDLCName), PackageContent(InPackageContent)
	{ }
	
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
#if PLATFORM_WINDOWS
		FString CommandLine = FString::Printf(TEXT("makeUTDLC -skipcook -DLCName=%s -platform=Win64 -version=%d"), *DLCName, FNetworkVersion::GetLocalNetworkVersion());
#elif PLATFORM_LINUX
		FString CommandLine = FString::Printf(TEXT("makeUTDLC -skipcook -DLCName=%s -platform=Linux -version=%d"), *DLCName, FNetworkVersion::GetLocalNetworkVersion());
#else
		FString CommandLine = FString::Printf(TEXT("makeUTDLC -skipcook -DLCName=%s -platform=Mac -version=%d"), *DLCName, FNetworkVersion::GetLocalNetworkVersion());
#endif
		
		PackageContent->CreateUATTask(CommandLine, DLCName, LOCTEXT("CookingTaskName", "Publishing"), LOCTEXT("CookingTaskName", "Publishing"), FEditorStyle::GetBrush(TEXT("MainFrame.CookContent")));
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPackageContentInEditorPart2Task, STATGROUP_TaskGraphTasks);
	}

private:

	FString DLCName;
	FPackageContent* PackageContent;
};

class FPackageContentInEditorTask
{
public:

	FPackageContentInEditorTask(const FString& InDLCName, FPackageContent* InPackageContent)
		: DLCName(InDLCName), PackageContent(InPackageContent)
	{ }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		// We're not on the game thread, don't try to access slate or anything fancy here
		while (GUnrealEd->CookServer->IsCookByTheBookRunning() && !GUnrealEd->IsCookByTheBookInEditorFinished())
		{
		}

		// packaging complete, now do makeUAT job to package it
		TGraphTask<FPackageContentInEditorPart2Task>::CreateTask().ConstructAndDispatchWhenReady(DLCName, PackageContent);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPackageContentInEditorTask, STATGROUP_TaskGraphTasks);
	}

private:

	FString DLCName;
	FPackageContent* PackageContent;
};

class FPackageContentCompleteTask
{
public:

	FPackageContentCompleteTask(const FString& InDLCName)
		: DLCName(InDLCName)
	{ }
	
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		if (DesktopPlatform != nullptr)
		{
#if PLATFORM_WINDOWS
			// Can't use UserDir here as editor may have been run with -installed, UAT will save to GameDir()/Saved
			FString PakPath = FPaths::ConvertRelativePathToFull(FPaths::GameDir() / TEXT("Saved") / TEXT("StagedBuilds") / DLCName / TEXT("WindowsNoEditor") / TEXT("UnrealTournament") / TEXT("Content") / TEXT("Paks") / DLCName + TEXT("-WindowsNoEditor.pak"));
			
			// Copy to game directory for now, launcher may do this for us later
			FString DestinationPath = FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::UserDir()) / FApp::GetGameName() / TEXT("Saved") / TEXT("Paks") / TEXT("MyContent") / DLCName + "-WindowsNoEditor.pak");
#elif PLATFORM_LINUX
			FString PakPath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() / TEXT("StagedBuilds") / DLCName / TEXT("LinuxNoEditor") / TEXT("UnrealTournament") / TEXT("Content") / TEXT("Paks") / DLCName + TEXT("-LinuxNoEditor.pak"));

			// Copy to game directory for now, launcher may do this for us later
			FString DestinationPath = FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::UserDir()) / FApp::GetGameName() / TEXT("Saved") / TEXT("Paks") / TEXT("MyContent") / DLCName + "-LinuxNoEditor.pak");
#else
			FString PakPath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() / TEXT("StagedBuilds") / DLCName / TEXT("MacNoEditor") / TEXT("UnrealTournament") / TEXT("Content") / TEXT("Paks") / DLCName + TEXT("-MacNoEditor.pak"));

			// Copy to game directory for now, launcher may do this for us later
			FString DestinationPath = FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::UserDir()) / FApp::GetGameName() / TEXT("Saved") / TEXT("Paks") / TEXT("MyContent") / DLCName + "-MacNoEditor.pak");
#endif
			if (IFileManager::Get().Copy(*DestinationPath, *PakPath, true) != COPY_OK)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CopyPackageFailed", "Could not copy completed package file to game directory."));
				return;
			}

			TSharedPtr<SWindow> MsgWindow = NULL;
			TSharedPtr<SPackageCompleteChoiceDialog> MsgDialog = NULL;

			MsgWindow = SNew(SWindow)
				.Title(NSLOCTEXT("PublishContentSuccessTitle", "MessageTitle", "Would you like to share?"))
				.SizingRule(ESizingRule::Autosized)
				.AutoCenter(EAutoCenter::PreferredWorkArea)
				.SupportsMinimize(false).SupportsMaximize(false);

			MsgDialog = SNew(SPackageCompleteChoiceDialog)
				.ParentWindow(MsgWindow)
				.Message(LOCTEXT("PublishContentSuccess", "Content packaged successfully. Would you like to share it now?\n\nIf you share this content, it will be uploaded to UT File Storage so it can be shared with your friends if you start a game server using this content."))
				.WrapMessageAt(512.0f)
				.FilePath(DestinationPath)
				.FilePathLinkText(LOCTEXT("PublishContentSuccessOpenWindow", "Open Window To Packaged Content Directory"))
				.MessageType(EAppMsgType::YesNo);

			MsgDialog->ResultCallback = nullptr;

			MsgWindow->SetContent(MsgDialog.ToSharedRef());

			GEditor->EditorAddModalWindow(MsgWindow.ToSharedRef());

			EAppReturnType::Type Response = MsgDialog->GetResponse();

			if (EAppReturnType::Yes == Response)
			{
				FString LauncherCommandLine = TEXT("-assetuploadcategory=ut -assetuploadpath=\"") + PakPath + TEXT("\"");

				FString McpConfigOverride;
				FParse::Value(FCommandLine::Get(), TEXT("EPICENV="), McpConfigOverride);
				if (McpConfigOverride.IsEmpty())
				{
					FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride);
				}
				if (McpConfigOverride == TEXT("gamedev"))
				{
					LauncherCommandLine += TEXT(" -mcpconfig=gamedev -epicenv=gamedev -Launcherlabel=LatestLauncherDev -applabel=Production-Latest");
				}

				FOpenLauncherOptions OpenLauncherOptions;
				OpenLauncherOptions.bInstall = false;
				OpenLauncherOptions.LauncherRelativeUrl = LauncherCommandLine;

				if (DesktopPlatform->OpenLauncher(OpenLauncherOptions))
				{

				}
				else
				{
					if (EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("InstallLauncherPrompt", "Sharing content requires the Epic Games Launcher, which does not seem to be installed on your computer. Would you like to install it now?")))
					{
						if (!DesktopPlatform->OpenLauncher(OpenLauncherOptions))
						{
						}
					}
				}
			}
		}
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPackageContentCompleteTask, STATGROUP_TaskGraphTasks);
	}

private:

	FString DLCName;
};

void FPackageContent::CreateUATTask(const FString& CommandLine, const FString& DLCName, const FText& TaskName, const FText &TaskShortName, const FSlateBrush* TaskIcon)
{
	// make sure that the UAT batch file is in place
#if PLATFORM_WINDOWS
	FString RunUATScriptName = TEXT("RunUAT.bat");
	FString CmdExe = TEXT("cmd.exe");
#elif PLATFORM_LINUX
	FString RunUATScriptName = TEXT("RunUAT.sh");
	FString CmdExe = TEXT("/bin/bash");
#else
	FString RunUATScriptName = TEXT("RunUAT.command");
	FString CmdExe = TEXT("/bin/sh");
#endif

	FString UatPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles") / RunUATScriptName);

#if PLATFORM_WINDOWS
	FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *UatPath, *CommandLine);
#else
	FString FullCommandLine = FString::Printf(TEXT("\"%s\" %s"), *UatPath, *CommandLine);
#endif

	UE_LOG(LogPackageContent, Log, TEXT("Starting UAT Task: %s"), *FullCommandLine);

	TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));

	// create notification item
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TaskName"), TaskName);
	FNotificationInfo Info(FText::Format(LOCTEXT("UatTaskInProgressNotification", "{TaskName}..."), Arguments));

	Info.Image = TaskIcon;
	Info.bFireAndForget = false;
	Info.ExpireDuration = 3.0f;
	Info.Hyperlink = FSimpleDelegate::CreateStatic(&FPackageContent::HandleUatHyperlinkNavigate);
	Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
	Info.ButtonDetails.Add(
		FNotificationButtonInfo(
		LOCTEXT("UatTaskCancel", "Cancel"),
		LOCTEXT("UatTaskCancelToolTip", "Cancels execution of this task."),
		FSimpleDelegate::CreateStatic(&FPackageContent::HandleUatCancelButtonClicked, UatProcess)
		)
		);

	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

	if (!NotificationItem.IsValid())
	{
		return;
	}
	
	NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);

	// launch the packager
	TWeakPtr<SNotificationItem> NotificationItemPtr(NotificationItem);

	EventData Data;
	Data.StartTime = FPlatformTime::Seconds();
	Data.EventName = TaskName.ToString();
	Data.DLCName = DLCName;
	UatProcess->OnCanceled().BindStatic(&FPackageContent::HandleUatProcessCanceled, NotificationItemPtr, TaskShortName, Data);
	UatProcess->OnCompleted().BindStatic(&FPackageContent::HandleUatProcessCompleted, NotificationItemPtr, TaskShortName, Data);
	UatProcess->OnOutput().BindStatic(&FPackageContent::HandleUatProcessOutput, NotificationItemPtr, TaskShortName);

	if (UatProcess->Launch())
	{
		GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileStart_Cue.CompileStart_Cue"));
	}
	else
	{
		GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));

		NotificationItem->SetText(LOCTEXT("UatLaunchFailedNotification", "Failed to launch Unreal Automation Tool (UAT)!"));
		NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		NotificationItem->ExpireAndFadeout();
	}
}

// Handles clicking the packager notification item's Cancel button.
void FPackageContent::HandleUatCancelButtonClicked(TSharedPtr<FMonitoredProcess> PackagerProcess)
{
	if (PackagerProcess.IsValid())
	{
		PackagerProcess->Cancel(true);
	}
}

// Handles clicking the hyper link on a packager notification item.
void FPackageContent::HandleUatHyperlinkNavigate()
{
	FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog"));
}

// Handles canceled packager processes.
void FPackageContent::HandleUatProcessCanceled(TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName, EventData Event)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TaskName"), TaskName);

	TGraphTask<FPackageContentNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
		NotificationItemPtr,
		SNotificationItem::CS_Fail,
		FText::Format(LOCTEXT("UatProcessFailedNotification", "{TaskName} canceled!"), Arguments)
		);
}

// Handles the completion of a packager process.
void FPackageContent::HandleUatProcessCompleted(int32 ReturnCode, TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName, EventData Event)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TaskName"), TaskName);

	if (ReturnCode == 0)
	{
		TGraphTask<FPackageContentNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Success,
			FText::Format(LOCTEXT("UatProcessSucceededNotification", "{TaskName} complete!"), Arguments)
			);

		// Run this on the main thread
		TGraphTask<FPackageContentCompleteTask>::CreateTask().ConstructAndDispatchWhenReady(Event.DLCName);		
	}
	else
	{
		TGraphTask<FPackageContentNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Fail,
			FText::Format(LOCTEXT("PackagerFailedNotification", "{TaskName} failed!"), Arguments)
			);
	}
}

// Handles packager process output.
void FPackageContent::HandleUatProcessOutput(FString Output, TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName)
{
	if (!Output.IsEmpty() && !Output.Equals("\r"))
	{
		UE_LOG(LogPackageContent, Log, TEXT("%s: %s"), *TaskName.ToString(), *Output);
	}
}

void FPackageContent::PackageWeapon(UClass* WeaponClass)
{
	UBlueprintGeneratedClass* UBGC = Cast<UBlueprintGeneratedClass>(WeaponClass);
	if (UBGC && UBGC->ClassGeneratedBy)
	{
		FString WeaponName = UBGC->ClassGeneratedBy->GetName();

		PackageDLC(WeaponName, LOCTEXT("PackageWeaponTaskName", "Packaging Weapon"), LOCTEXT("CookingTaskName", "Publishing"));
	}
}

void FPackageContent::PackageDLC(const FString& DLCName, const FText& TaskName, const FText& TaskShortName)
{
	UUTUnrealEdEngine* UTEditor = Cast<UUTUnrealEdEngine>(GEditor);
	// Disabled for now, doesn't seem faster
	if (0 && UTEditor && UTEditor->CanCookByTheBookInEditor("WindowsNoEditor"))
	{
		TArray<ITargetPlatform*> TargetPlatforms;
		ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatform("WindowsNoEditor");
		TargetPlatforms.Add(TargetPlatform);
		UTEditor->StartDLCCookInEditor(TargetPlatforms, DLCName, "UTVersion0");
		
		TGraphTask<FPackageContentInEditorTask>::CreateTask().ConstructAndDispatchWhenReady(DLCName, this);
	}
	else
	{
#if PLATFORM_WINDOWS
		FString CommandLine = FString::Printf(TEXT("makeUTDLC -DLCName=%s -platform=Win64 -version=%d"), *DLCName, FNetworkVersion::GetLocalNetworkVersion());
#elif PLATFORM_LINUX
		FString CommandLine = FString::Printf(TEXT("makeUTDLC -DLCName=%s -platform=Linux -version=%d"), *DLCName, FNetworkVersion::GetLocalNetworkVersion());
#else
		FString CommandLine = FString::Printf(TEXT("makeUTDLC -DLCName=%s -platform=Mac -version=%d"), *DLCName, FNetworkVersion::GetLocalNetworkVersion());
#endif

		CreateUATTask(CommandLine, DLCName, TaskName, TaskShortName, FEditorStyle::GetBrush(TEXT("MainFrame.CookContent")));
	}
}

void FPackageContent::PackageHat(UClass* HatClass)
{
	UBlueprintGeneratedClass* UBGC = Cast<UBlueprintGeneratedClass>(HatClass);
	if (UBGC && UBGC->ClassGeneratedBy)
	{
		FString HatName = UBGC->ClassGeneratedBy->GetName();

		PackageDLC(HatName, LOCTEXT("PackageCosmeticTaskName", "Packaging Cosmetic Item"), LOCTEXT("CookingTaskName", "Publishing"));
	}
}

void FPackageContent::PackageTaunt(UClass* TauntClass)
{
	UBlueprintGeneratedClass* UBGC = Cast<UBlueprintGeneratedClass>(TauntClass);
	if (UBGC && UBGC->ClassGeneratedBy)
	{
		FString TauntName = UBGC->ClassGeneratedBy->GetName();
		PackageDLC(TauntName, LOCTEXT("PackageTauntTaskName", "Packaging Taunt"), LOCTEXT("CookingTaskName", "Publishing"));
	}
}

void FPackageContent::PackageMutator(UClass* MutatorClass)
{
	UBlueprintGeneratedClass* UBGC = Cast<UBlueprintGeneratedClass>(MutatorClass);
	if (UBGC && UBGC->ClassGeneratedBy)
	{
		FString MutatorName = UBGC->ClassGeneratedBy->GetName();
		PackageDLC(MutatorName, LOCTEXT("PackageMutatorTaskName", "Packaging Mutator"), LOCTEXT("CookingTaskName", "Publishing"));
	}
}

void FPackageContent::PackageCharacter(UClass* CharacterClass)
{
	UBlueprintGeneratedClass* UBGC = Cast<UBlueprintGeneratedClass>(CharacterClass);
	if (UBGC && UBGC->ClassGeneratedBy)
	{
		FString CharacterName = UBGC->ClassGeneratedBy->GetName();
		PackageDLC(CharacterName, LOCTEXT("PackageCharacterTaskName", "Packaging Character"), LOCTEXT("CookingTaskName", "Publishing"));
	}
}

void FPackageContent::PackageCrosshair(UClass* CrosshairClass)
{
	UBlueprintGeneratedClass* UBGC = Cast<UBlueprintGeneratedClass>(CrosshairClass);
	if (UBGC && UBGC->ClassGeneratedBy)
	{
		FString CrosshairName = UBGC->ClassGeneratedBy->GetName();
		PackageDLC(CrosshairName, LOCTEXT("PackageCrosshairTaskName", "Packaging Crosshair"), LOCTEXT("CookingTaskName", "Publishing"));
	}
}

void FPackageContent::OpenPackageCharacterWindow()
{
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = false;
	bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		PackageDialogTitle = LOCTEXT("PackageCharacterDialogTitle", "Share A Character");

		/** Create the window to host our package dialog widget */
		TSharedRef< SWindow > EditorPackageCharacterDialogWindowRef = SNew(SWindow)
			.Title(PackageDialogTitle)
			.ClientSize(FPackageContent::DEFAULT_WINDOW_SIZE);

		TSharedPtr<class SPackageContentDialog> PackagesDialogWidget = SNew(SPackageContentDialog, EditorPackageCharacterDialogWindowRef)
			.PackageContent(SharedThis(this))
			.DialogMode(SPackageContentDialog::EPackageContentDialogMode::PACKAGE_Character);

		// Set the content of the window to our package dialog widget
		EditorPackageCharacterDialogWindowRef->SetContent(PackagesDialogWidget.ToSharedRef());

		// Show the package dialog window as a modal window
		GEditor->EditorAddModalWindow(EditorPackageCharacterDialogWindowRef);
	}
}

void FPackageContent::OpenPackageMutatorWindow()
{
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = false;
	bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		PackageDialogTitle = LOCTEXT("PackageMutatorDialogTitle", "Share A Mutator");

		/** Create the window to host our package dialog widget */
		TSharedRef< SWindow > EditorPackageMutatorDialogWindowRef = SNew(SWindow)
			.Title(PackageDialogTitle)
			.ClientSize(FPackageContent::DEFAULT_WINDOW_SIZE);

		TSharedPtr<class SPackageContentDialog> PackagesDialogWidget = SNew(SPackageContentDialog, EditorPackageMutatorDialogWindowRef)
			.PackageContent(SharedThis(this))
			.DialogMode(SPackageContentDialog::EPackageContentDialogMode::PACKAGE_Mutator);

		// Set the content of the window to our package dialog widget
		EditorPackageMutatorDialogWindowRef->SetContent(PackagesDialogWidget.ToSharedRef());

		// Show the package dialog window as a modal window
		GEditor->EditorAddModalWindow(EditorPackageMutatorDialogWindowRef);
	}
}

void FPackageContent::OpenPackageTauntWindow()
{
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = false;
	bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		PackageDialogTitle = LOCTEXT("PackageTauntDialogTitle", "Share A Taunt");

		/** Create the window to host our package dialog widget */
		TSharedRef< SWindow > EditorPackageTauntDialogWindowRef = SNew(SWindow)
			.Title(PackageDialogTitle)
			.ClientSize(FPackageContent::DEFAULT_WINDOW_SIZE);

		TSharedPtr<class SPackageContentDialog> PackagesDialogWidget = SNew(SPackageContentDialog, EditorPackageTauntDialogWindowRef)
			.PackageContent(SharedThis(this))
			.DialogMode(SPackageContentDialog::EPackageContentDialogMode::PACKAGE_Taunt);

		// Set the content of the window to our package dialog widget
		EditorPackageTauntDialogWindowRef->SetContent(PackagesDialogWidget.ToSharedRef());

		// Show the package dialog window as a modal window
		GEditor->EditorAddModalWindow(EditorPackageTauntDialogWindowRef);
	}
}

void FPackageContent::OpenPackageWeaponWindow()
{
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = false;
	bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		PackageDialogTitle = LOCTEXT("PackageWeaponDialogTitle", "Share A Weapon");

		/** Create the window to host our package dialog widget */
		TSharedRef< SWindow > EditorPackageWeaponDialogWindowRef = SNew(SWindow)
			.Title(PackageDialogTitle)
			.ClientSize(FPackageContent::DEFAULT_WINDOW_SIZE);
	
		TSharedPtr<class SPackageContentDialog> PackagesDialogWidget = SNew(SPackageContentDialog, EditorPackageWeaponDialogWindowRef)
																	  .PackageContent(SharedThis(this))
																	  .DialogMode(SPackageContentDialog::EPackageContentDialogMode::PACKAGE_Weapon);
	
		// Set the content of the window to our package dialog widget
		EditorPackageWeaponDialogWindowRef->SetContent(PackagesDialogWidget.ToSharedRef());
	
		// Show the package dialog window as a modal window
		GEditor->EditorAddModalWindow(EditorPackageWeaponDialogWindowRef);
	}
}

void FPackageContent::OpenPackageHatWindow()
{
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = false;
	bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		PackageDialogTitle = LOCTEXT("PackageHatDialogTitle", "Share A Cosmetic Item");

		/** Create the window to host our package dialog widget */
		TSharedRef< SWindow > EditorPackageWeaponDialogWindowRef = SNew(SWindow)
			.Title(PackageDialogTitle)
			.ClientSize(FPackageContent::DEFAULT_WINDOW_SIZE);

		TSharedPtr<class SPackageContentDialog> PackagesDialogWidget = SNew(SPackageContentDialog, EditorPackageWeaponDialogWindowRef)
			.PackageContent(SharedThis(this))
			.DialogMode(SPackageContentDialog::EPackageContentDialogMode::PACKAGE_Hat);

		// Set the content of the window to our package dialog widget
		EditorPackageWeaponDialogWindowRef->SetContent(PackagesDialogWidget.ToSharedRef());

		// Show the package dialog window as a modal window
		GEditor->EditorAddModalWindow(EditorPackageWeaponDialogWindowRef);
	}
}

void FPackageContent::OpenPackageCrosshairWindow()
{
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = false;
	bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		PackageDialogTitle = LOCTEXT("PackageCrosshairDialogTitle", "Share A Crosshair");

		/** Create the window to host our package dialog widget */
		TSharedRef< SWindow > EditorPackageWeaponDialogWindowRef = SNew(SWindow)
			.Title(PackageDialogTitle)
			.ClientSize(FPackageContent::DEFAULT_WINDOW_SIZE);

		TSharedPtr<class SPackageContentDialog> PackagesDialogWidget = SNew(SPackageContentDialog, EditorPackageWeaponDialogWindowRef)
			.PackageContent(SharedThis(this))
			.DialogMode(SPackageContentDialog::EPackageContentDialogMode::PACKAGE_Crosshair);

		// Set the content of the window to our package dialog widget
		EditorPackageWeaponDialogWindowRef->SetContent(PackagesDialogWidget.ToSharedRef());

		// Show the package dialog window as a modal window
		GEditor->EditorAddModalWindow(EditorPackageWeaponDialogWindowRef);
	}
}

void FPackageContent::CreatePackageContentMenu(FToolBarBuilder& Builder)
{
	Builder.AddComboButton(FUIAction(),	FOnGetContent::CreateSP(this, &FPackageContent::GenerateOpenPackageMenuContent),
						   LOCTEXT("PackageContent_Label", "Share"), LOCTEXT("PackageContent_ToolTip", "Package your custom content to get it ready to share."),
		                   FSlateIcon(FPackageContentStyle::GetStyleSetName(), "PackageContent.PackageContent"));
}

TSharedRef<SWidget> FPackageContent::GenerateOpenPackageMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, ActionList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("PublishContent", "Publish Content"));
	{
#if PLATFORM_WINDOWS
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageLevel);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageWeapon);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageHat);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageCharacter);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageTaunt);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageMutator);
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().PackageCrosshair);
#else
		MenuBuilder.AddMenuEntry(FPackageContentCommands::Get().ComingSoon);
#endif
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SPackageContentDialog::Construct(const FArguments& InArgs, TSharedPtr<SWindow> InParentWindow)
{
	ParentWindow = InParentWindow;
	PackageContent = InArgs._PackageContent;
	DialogMode = InArgs._DialogMode;

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
			
	if (DialogMode == PACKAGE_Weapon)
	{
		TSharedPtr<FWeaponClassFilter> Filter = MakeShareable(new FWeaponClassFilter);
		Options.ClassFilter = Filter;
		Options.ViewerTitleString = FText::FromString(LOCTEXT("WeaponClassSelect", "Select Weapon Blueprint").ToString());
	}
	else if (DialogMode == PACKAGE_Hat)
	{
		TSharedPtr<FHatClassFilter> Filter = MakeShareable(new FHatClassFilter);
		Options.ClassFilter = Filter;
		Options.ViewerTitleString = FText::FromString(LOCTEXT("HatClassSelect", "Select Cosmetic Blueprint").ToString());
	}
	else if (DialogMode == PACKAGE_Taunt)
	{
		TSharedPtr<FTauntClassFilter> Filter = MakeShareable(new FTauntClassFilter);
		Options.ClassFilter = Filter;
		Options.ViewerTitleString = FText::FromString(LOCTEXT("TauntClassSelect", "Select Taunt Blueprint").ToString());
	}
	else if (DialogMode == PACKAGE_Character)
	{
		TSharedPtr<FCharacterClassFilter> Filter = MakeShareable(new FCharacterClassFilter);
		Options.ClassFilter = Filter;
		Options.ViewerTitleString = FText::FromString(LOCTEXT("CharacterClassSelect", "Select Character Blueprint").ToString());
	}
	else if (DialogMode == PACKAGE_Crosshair)
	{
		TSharedPtr<FCrosshairClassFilter> Filter = MakeShareable(new FCrosshairClassFilter);
		Options.ClassFilter = Filter;
		Options.ViewerTitleString = FText::FromString(LOCTEXT("CrosshairClassSelect", "Select Crosshair Blueprint").ToString());
	}
	else if (DialogMode == PACKAGE_Mutator)
	{
		TSharedPtr<FMutatorClassFilter> Filter = MakeShareable(new FMutatorClassFilter);
		Options.ClassFilter = Filter;
		Options.ViewerTitleString = FText::FromString(LOCTEXT("MutatorClassSelect", "Select Mutator Blueprint").ToString());		
	}
	else
	{
		UE_LOG(LogPackageContent, Warning, TEXT("Dialog mode not set for SPackageContentDialog!!"));
	}


	TSharedRef<SWidget> ClassPicker = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SPackageContentDialog::ClassChosen));
	
	TSharedRef<SBorder> ClassPickerBorder =
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			ClassPicker
		];

	this->ChildSlot
		[
			ClassPickerBorder
		];
}

void SPackageContentDialog::ClassChosen(UClass* ChosenClass)
{
	if (DialogMode == PACKAGE_Weapon)
	{
		PackageContent->PackageWeapon(ChosenClass);
	}
	else if (DialogMode == PACKAGE_Hat)
	{
		PackageContent->PackageHat(ChosenClass);
	}
	else if (DialogMode == PACKAGE_Character)
	{
		PackageContent->PackageCharacter(ChosenClass);
	}
	else if (DialogMode == PACKAGE_Taunt)
	{
		PackageContent->PackageTaunt(ChosenClass);
	}
	else if (DialogMode == PACKAGE_Crosshair)
	{
		PackageContent->PackageCrosshair(ChosenClass);
	}
	else if (DialogMode == PACKAGE_Mutator)
	{
		PackageContent->PackageMutator(ChosenClass);
	}
	else
	{
		// unhandled
		UE_LOG(LogPackageContent, Warning, TEXT("Dialog mode unhandled in ClassChosen!!"));
	}

	ParentWindow->RequestDestroyWindow();
}

#undef LOCTEXT_NAMESPACE