// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PackageContentPrivatePCH.h"
#include "PackageContent.h"
#include "LevelEditor.h"
#include "PackageContentCommands.h"
#include "PackageContentStyle.h"
#include "NotificationManager.h"
#include "SNotificationList.h"

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
	UE_LOG(LogPackageContent, Log, TEXT("Starting to package this level!"));
	
	FString MapName = GWorld->GetMapName();
	FString CommandLine = FString::Printf(TEXT("makeUTDLC -DLCName=%s -Maps=%s -platform=Win64"), *MapName, *MapName);

	CreateUATTask(CommandLine, LOCTEXT("PackageLevelTaskName", "Packaging Level"), LOCTEXT("CookingTaskName", "Packaging"), FEditorStyle::GetBrush(TEXT("MainFrame.CookContent")));
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

void FPackageContent::CreateUATTask(const FString& CommandLine, const FText& TaskName, const FText &TaskShortName, const FSlateBrush* TaskIcon)
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