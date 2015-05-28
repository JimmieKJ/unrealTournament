// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "MainFramePrivatePCH.h"
#include "MessageLog.h"
#include "SDockTab.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "GenericCommands.h"
#include "EngineBuildSettings.h"
#include "SourceCodeNavigation.h"
#include "SOutputLogDialog.h"

#include "Settings/EditorSettings.h"

#define LOCTEXT_NAMESPACE "MainFrameActions"

DEFINE_LOG_CATEGORY_STATIC(MainFrameActions, Log, All);


TSharedRef< FUICommandList > FMainFrameCommands::ActionList( new FUICommandList() );

TWeakPtr<SNotificationItem> FMainFrameActionCallbacks::ChoosePackagesToCheckInNotification;

FMainFrameCommands::FMainFrameCommands()
	: TCommands<FMainFrameCommands>(
		TEXT("MainFrame"), // Context name for fast lookup
		LOCTEXT( "MainFrame", "Main Frame" ), // Localized context name for displaying
		NAME_None,	 // No parent context
		FEditorStyle::GetStyleSetName() ), // Icon Style Set
	  ToggleFullscreenConsoleCommand(
		TEXT( "MainFrame.ToggleFullscreen" ),
		TEXT( "Toggles the editor between \"full screen\" mode and \"normal\" mode.  In full screen mode, the task bar and window title area are hidden." ),
		FConsoleCommandDelegate::CreateStatic( &FMainFrameActionCallbacks::ToggleFullscreen_Execute ) )
{ }


void FMainFrameCommands::RegisterCommands()
{
	if ( !IsRunningCommandlet() )
	{
		// The global action list was created at static initialization time. Create a handler for otherwise unhandled keyboard input to route key commands through this list.
		FSlateApplication::Get().SetUnhandledKeyDownEventHandler( FOnKeyEvent::CreateStatic( &FMainFrameActionCallbacks::OnUnhandledKeyDownEvent ) );
	}

	// Make a default can execute action that disables input when in debug mode
	FCanExecuteAction DefaultExecuteAction = FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::DefaultCanExecuteAction );

	UI_COMMAND( SaveAll, "Save All", "Saves all unsaved levels and assets to disk", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::S ) );
	ActionList->MapAction( SaveAll, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::SaveAll ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanSaveWorld ) );

	UI_COMMAND( ChooseFilesToSave, "Choose Files to Save...", "Opens a dialog with save options for content and levels", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( ChooseFilesToSave, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ChoosePackagesToSave ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanSaveWorld ) );

	UI_COMMAND( ChooseFilesToCheckIn, "Submit to Source Control...", "Opens a dialog with check in options for content and levels", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( ChooseFilesToCheckIn, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ChoosePackagesToCheckIn ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanChoosePackagesToCheckIn ) );

	UI_COMMAND( ConnectToSourceControl, "Connect To Source Control...", "Connect to source control to allow source control operations to be performed on content and levels.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( ConnectToSourceControl, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ConnectToSourceControl ), DefaultExecuteAction );

	UI_COMMAND( NewProject, "New Project...", "Opens a dialog to create a new game project", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( NewProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::NewProject, false, true), DefaultExecuteAction );

	UI_COMMAND( OpenProject, "Open Project...", "Opens a dialog to choose a game project to open", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( OpenProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::NewProject, true, false), DefaultExecuteAction );

	UI_COMMAND( AddCodeToProject, "New C++ Class...", "Adds C++ code to the project. The code can only be compiled if you have an appropriate C++ compiler installed.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( AddCodeToProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::AddCodeToProject ));

	UI_COMMAND( RefreshCodeProject, "Refresh code project", "Refreshes your C++ code project.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( RefreshCodeProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::RefreshCodeProject ), DefaultExecuteAction );

	UI_COMMAND( OpenIDE, "Open IDE", "Opens your C++ code in an integrated development environment.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( OpenIDE, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::OpenIDE ), DefaultExecuteAction );

	UI_COMMAND( PackagingSettings, "Packaging Settings...", "Opens the settings for project packaging", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( PackagingSettings, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::PackagingSettings ), DefaultExecuteAction );

	//UI_COMMAND( LocalizeProject, "Localize Project...", "Opens the dashboard for managing project localization data.", EUserInterfaceActionType::Button, FInputChord() );
	//ActionList->MapAction( LocalizeProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::LocalizeProject ), DefaultExecuteAction );

	const int32 MaxProjects = 20;
	for( int32 CurProjectIndex = 0; CurProjectIndex < MaxProjects; ++CurProjectIndex )
	{
		// NOTE: The actual label and tool-tip will be overridden at runtime when the command is bound to a menu item, however
		// we still need to set one here so that the key bindings UI can function properly
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("CurrentProjectIndex"), CurProjectIndex);
		const FText Message = FText::Format( LOCTEXT( "SwitchProject", "Switch Project {CurrentProjectIndex}" ), Arguments ); 
		TSharedRef< FUICommandInfo > SwitchProject =
			FUICommandInfoDecl(
				this->AsShared(),
				FName( *FString::Printf( TEXT( "SwitchProject%i" ), CurProjectIndex ) ),
				Message,
				LOCTEXT( "SwitchProjectToolTip", "Restarts the editor and switches to selected project" ) )
			.UserInterfaceType( EUserInterfaceActionType::Button )
			.DefaultChord( FInputChord() );
		SwitchProjectCommands.Add( SwitchProject );

		ActionList->MapAction( SwitchProjectCommands[ CurProjectIndex ], FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::SwitchProjectByIndex, CurProjectIndex ),
			FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanSwitchToProject, CurProjectIndex ),
			FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::IsSwitchProjectChecked, CurProjectIndex ) );
	}

	UI_COMMAND( Exit, "Exit", "Exits the application", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( Exit, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::Exit ), DefaultExecuteAction );

	ActionList->MapAction( FGenericCommands::Get().Undo, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ExecuteExecCommand, FString( TEXT("TRANSACTION UNDO") ) ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::Undo_CanExecute ) );

	ActionList->MapAction( FGenericCommands::Get().Redo, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ExecuteExecCommand, FString( TEXT("TRANSACTION REDO") ) ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::Redo_CanExecute ) );

	UI_COMMAND( OpenDeviceManagerApp, "Device Manager", "Opens up the device manager app", EUserInterfaceActionType::Check, FInputChord() );
	ActionList->MapAction( OpenDeviceManagerApp, 
												FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp, FName( TEXT( "DeviceManager" ) ) ),
												FCanExecuteAction(),
												FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp_IsChecked, FName( TEXT( "DeviceManager" ) ) ) );

	UI_COMMAND( OpenSessionManagerApp, "Session Manager", "Opens up the session manager app", EUserInterfaceActionType::Check, FInputChord() );
	ActionList->MapAction( OpenSessionManagerApp, 
												FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp, FName( "SessionFrontend" ) ),
												FCanExecuteAction(),
												FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp_IsChecked, FName("SessionFrontend" ) ) );

	UI_COMMAND(VisitWiki, "Wiki...", "Go to the Unreal Engine Wiki page", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitWiki, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitWiki));

	UI_COMMAND(VisitForums, "Forums...", "Go to the Unreal Engine forums", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitForums, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitForums));

	UI_COMMAND( VisitAskAQuestionPage, "Ask a Question...", "Have a question?  Go here to ask about anything and everything related to Unreal.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( VisitAskAQuestionPage, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::VisitAskAQuestionPage ) );

	UI_COMMAND( VisitSearchForAnswersPage, "Answer Hub...", "Searches for useful answers on UDN provided by other users and experts.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( VisitSearchForAnswersPage, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::VisitSearchForAnswersPage ) );

	UI_COMMAND( VisitSupportWebSite, "Unreal Engine Support Web Site...", "Navigates to the Unreal Engine Support web site's main page.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( VisitSupportWebSite, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::VisitSupportWebSite ) );

	UI_COMMAND( VisitEpicGamesDotCom, "Visit UnrealEngine.com...", "Navigates to UnrealEngine.com where you can learn more about Unreal Technology.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( VisitEpicGamesDotCom, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::VisitEpicGamesDotCom ) );

	UI_COMMAND( AboutUnrealEd, "About Editor...", "Displays application credits and copyright information", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( AboutUnrealEd, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::AboutUnrealEd_Execute ) );

	UI_COMMAND( CreditsUnrealEd, "Credits", "Displays application credits", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( CreditsUnrealEd, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CreditsUnrealEd_Execute) );

	UI_COMMAND( ResetLayout, "Reset Layout...", "Make a backup of your user settings and reset the layout customizations", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( ResetLayout, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ResetLayout) );

	UI_COMMAND( SaveLayout, "Save Layout", "Save the layout customizations", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( SaveLayout, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::SaveLayout) );

	UI_COMMAND( ToggleFullscreen, "Enable Fullscreen", "Enables fullscreen mode for the application, expanding across the entire monitor", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::F11) );
	ActionList->MapAction( ToggleFullscreen,
		FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ToggleFullscreen_Execute ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::FullScreen_IsChecked )
	);

	UI_COMMAND(OpenWidgetReflector, "Open Widget Reflector", "Opens the Widget Reflector", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Control , EKeys::W));
	ActionList->MapAction(OpenWidgetReflector, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::OpenWidgetReflector_Execute));

	FGlobalEditorCommonCommands::MapActions(ActionList);
}

FReply FMainFrameActionCallbacks::OnUnhandledKeyDownEvent(const FKeyEvent& InKeyEvent)
{
	if ( FMainFrameCommands::ActionList->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

bool FMainFrameActionCallbacks::DefaultCanExecuteAction()
{
	return FSlateApplication::Get().IsNormalExecution();
}

void FMainFrameActionCallbacks::ChoosePackagesToSave()
{
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bClosingEditor = false;
	const bool bNotifyNoPackagesSaved = true;
	const bool bCanBeDeclined = false;
	FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined );
}

void FMainFrameActionCallbacks::ChoosePackagesToCheckIn()
{
	FSourceControlWindows::ChoosePackagesToCheckIn();
}

bool FMainFrameActionCallbacks::CanChoosePackagesToCheckIn()
{
	return FSourceControlWindows::CanChoosePackagesToCheckIn();
}

void FMainFrameActionCallbacks::ConnectToSourceControl()
{
	ELoginWindowMode::Type Mode = !FSlateApplication::Get().GetActiveModalWindow().IsValid() ? ELoginWindowMode::Modeless : ELoginWindowMode::Modal;
	ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), Mode);
}

bool FMainFrameActionCallbacks::CanSaveWorld()
{
	return FSlateApplication::Get().IsNormalExecution() && (!GUnrealEd || !GUnrealEd->GetPackageAutoSaver().IsAutoSaving());
}

void FMainFrameActionCallbacks::SaveAll()
{
	const bool bPromptUserToSave = false;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = false;
	FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined );
}

TArray<FString> FMainFrameActionCallbacks::ProjectNames;

void FMainFrameActionCallbacks::CacheProjectNames()
{
	ProjectNames.Empty();

	// The switch project menu is filled with recently opened project files
	ProjectNames = GetDefault<UEditorSettings>()->RecentlyOpenedProjectFiles;
}

void FMainFrameActionCallbacks::NewProject( bool bAllowProjectOpening, bool bAllowProjectCreate )
{
	if (GUnrealEd->WarnIfLightingBuildIsCurrentlyRunning())
	{
		return;
	}

	FText Title;
	if (bAllowProjectOpening && bAllowProjectCreate)
	{
		Title = LOCTEXT( "SelectProjectWindowHeader", "Select Project");
	}
	else if (bAllowProjectOpening)
	{
		Title = LOCTEXT( "OpenProjectWindowHeader", "Open Project");
	}
	else
	{
		Title = LOCTEXT( "NewProjectWindowHeader", "New Project");
	}

	TSharedRef<SWindow> NewProjectWindow =
		SNew(SWindow)
		.Title(Title)
		.ClientSize( FMainFrameModule::GetProjectBrowserWindowSize() )
		.SizingRule( ESizingRule::UserSized )
		.SupportsMinimize(false) .SupportsMaximize(false);

	NewProjectWindow->SetContent( FGameProjectGenerationModule::Get().CreateGameProjectDialog(bAllowProjectOpening, bAllowProjectCreate) );

	IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(NewProjectWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(NewProjectWindow);
	}
}

void FMainFrameActionCallbacks::AddCodeToProject()
{
	FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog();
}

/**
 * Gets compilation flags for UAT for this system.
 */
const TCHAR* GetUATCompilationFlags()
{
	return FRocketSupport::IsRocket() || !FSourceCodeNavigation::IsCompilerAvailable()
		? TEXT("-nocompile")
		: TEXT("-nocompileeditor");
}

void FMainFrameActionCallbacks::CookContent(const FName InPlatformInfoName)
{
	const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(InPlatformInfoName);
	check(PlatformInfo);

	FString OptionalParams;

	if (!FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor").ShowUnsupportedTargetWarning(PlatformInfo->VanillaPlatformName))
	{
		return;
	}

	if (PlatformInfo->SDKStatus == PlatformInfo::EPlatformSDKStatus::NotInstalled)
	{
		IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.BroadcastMainFrameSDKNotInstalled(PlatformInfo->TargetPlatformName.ToString(), PlatformInfo->SDKTutorial);
		return;
	}

	// Append any extra UAT flags specified for this platform flavor
	if (!PlatformInfo->UATCommandLine.IsEmpty())
	{
		OptionalParams += TEXT(" ");
		OptionalParams += PlatformInfo->UATCommandLine;
	}
	else
	{
		OptionalParams += TEXT(" -targetplatform=");
		OptionalParams += *PlatformInfo->TargetPlatformName.ToString();
	}

	const bool bRunningDebug = FParse::Param(FCommandLine::Get(), TEXT("debug"));

	if (bRunningDebug)
	{
		OptionalParams += TEXT(" -UseDebugParamForEditorExe");
	}

	FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()) : FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
	FString CommandLine = FString::Printf(TEXT("BuildCookRun %s%s%s -nop4 -project=\"%s\" -cook -ue4exe=%s %s"),
		FRocketSupport::IsRocket() ? TEXT("-rocket ") : TEXT(""),
		GetUATCompilationFlags(),
		FApp::IsEngineInstalled() ? TEXT(" -installed") : TEXT(""),
		*ProjectPath,
		*FUnrealEdMisc::Get().GetExecutableForCommandlets(),
		*OptionalParams
	);

	CreateUatTask(CommandLine, PlatformInfo->DisplayName, LOCTEXT("CookingContentTaskName", "Cooking content"), LOCTEXT("CookingTaskName", "Cooking"), FEditorStyle::GetBrush(TEXT("MainFrame.CookContent")));
}

bool FMainFrameActionCallbacks::CookContentCanExecute( const FName PlatformInfoName )
{
	return true;
}

void FMainFrameActionCallbacks::PackageBuildConfiguration( EProjectPackagingBuildConfigurations BuildConfiguration )
{
	UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
	PackagingSettings->BuildConfiguration = BuildConfiguration;
}

bool FMainFrameActionCallbacks::CanPackageBuildConfiguration( EProjectPackagingBuildConfigurations BuildConfiguration )
{
	UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
	if (PackagingSettings->ForDistribution && BuildConfiguration != PPBC_Shipping)
	{
		return false;
	}
	return true;
}

bool FMainFrameActionCallbacks::PackageBuildConfigurationIsChecked( EProjectPackagingBuildConfigurations BuildConfiguration )
{
	return (GetDefault<UProjectPackagingSettings>()->BuildConfiguration == BuildConfiguration);
}

void FMainFrameActionCallbacks::PackageProject( const FName InPlatformInfoName )
{
	// does the project have any code?
	FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
	bool bProjectHasCode = GameProjectModule.Get().ProjectRequiresBuild(InPlatformInfoName);

	const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(InPlatformInfoName);
	check(PlatformInfo);

	if (PlatformInfo->SDKStatus == PlatformInfo::EPlatformSDKStatus::NotInstalled || (bProjectHasCode && PlatformInfo->bUsesHostCompiler && !FSourceCodeNavigation::IsCompilerAvailable()))
	{
		IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.BroadcastMainFrameSDKNotInstalled(PlatformInfo->TargetPlatformName.ToString(), PlatformInfo->SDKTutorial);
		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), 0.0));
		FEditorAnalytics::ReportEvent(TEXT("Editor.Package.Failed"), PlatformInfo->TargetPlatformName.ToString(), bProjectHasCode, EAnalyticsErrorCodes::SDKNotFound, ParamArray);
		return;
	}

	{
		const ITargetPlatform* const Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatformInfo->TargetPlatformName.ToString());
		if (Platform)
		{
			FString NotInstalledTutorialLink;
			FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()) : FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
			int32 Result = Platform->CheckRequirements(ProjectPath, bProjectHasCode, NotInstalledTutorialLink);

			// report to analytics
			FEditorAnalytics::ReportBuildRequirementsFailure(TEXT("Editor.Package.Failed"), PlatformInfo->TargetPlatformName.ToString(), bProjectHasCode, Result);

			// report to message log
			if ((Result & ETargetPlatformReadyStatus::SDKNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("SdkNotFoundMessage", "Software Development Kit (SDK) not found."),
					FText::Format(LOCTEXT("SdkNotFoundMessageDetail", "Please install the SDK for the {0} target platform!"), Platform->DisplayName()),
					NotInstalledTutorialLink
				);
			}

			if ((Result & ETargetPlatformReadyStatus::ProvisionNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("ProvisionNotFoundMessage", "Provision not found."),
					LOCTEXT("ProvisionNotFoundMessageDetail", "A provision is required for deploying your app to the device."),
					NotInstalledTutorialLink
				);
			}

			if ((Result & ETargetPlatformReadyStatus::SigningKeyNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("SigningKeyNotFoundMessage", "Signing key not found."),
					LOCTEXT("SigningKeyNotFoundMessageDetail", "The app could not be digitally signed, because the signing key is not configured."),
					NotInstalledTutorialLink
				);
			}

			if ((Result & ETargetPlatformReadyStatus::ManifestNotFound) != 0)
			{
				AddMessageLog(
					LOCTEXT("ManifestNotFound", "Manifest not found."),
					LOCTEXT("ManifestNotFoundMessageDetail", "The generated application manifest could not be found."),
					NotInstalledTutorialLink
					);
			}

			// report to main frame
			bool UnrecoverableError = false;

			if ((Result & ETargetPlatformReadyStatus::CodeUnsupported) != 0)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NotSupported", "Sorry, packaging a code-based project for the selected platform is currently not supported. This feature may be available in a future release."));
				UnrecoverableError = true;
			}
			else if ((Result & ETargetPlatformReadyStatus::PluginsUnsupported) != 0)
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NotSupported", "Sorry, packaging a project with third-party plugins is currently not supported for the selected platform. This feature may be available in a future release."));
				UnrecoverableError = true;
			}

			if (UnrecoverableError)
			{
				return;
			}
		}
	}

	if (!FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor").ShowUnsupportedTargetWarning(PlatformInfo->VanillaPlatformName))
	{
		return;
	}

	UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());

	// let the user pick a target directory
	if (PackagingSettings->StagingDirectory.Path.IsEmpty())
	{
		PackagingSettings->StagingDirectory.Path = FPaths::GameDir();
	}

	FString OutFolderName;

	void* ParentWindowWindowHandle = nullptr;
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
	{
		ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
	}
	
	if (!FDesktopPlatformModule::Get()->OpenDirectoryDialog(ParentWindowWindowHandle, LOCTEXT("PackageDirectoryDialogTitle", "Package project...").ToString(), PackagingSettings->StagingDirectory.Path, OutFolderName))
	{
		return;
	}

	PackagingSettings->StagingDirectory.Path = OutFolderName;
	PackagingSettings->SaveConfig();

	// create the packager process
	FString OptionalParams;
	
	if (PackagingSettings->FullRebuild)
	{
		OptionalParams += TEXT(" -clean");
	}

	if ( PackagingSettings->bCompressed )
	{
		OptionalParams += TEXT(" -compressed");
	}

	if ( PackagingSettings->bCookAll )
	{
		OptionalParams += TEXT(" -CookAll");
		// maps only flag only affects cook all
		if ( PackagingSettings->bCookMapsOnly )
		{
			OptionalParams += TEXT(" -CookMapsOnly");
		}
	}

	if (PackagingSettings->UsePakFile)
	{
	  if (PlatformInfo->TargetPlatformName != FName("HTML5")) 
	  { 
		OptionalParams += TEXT(" -pak");
	  }
	}

	if (PackagingSettings->IncludePrerequisites)
	{
		OptionalParams += TEXT(" -prereqs");
	}

	if (PackagingSettings->ForDistribution)
	{
		OptionalParams += TEXT(" -distribution");
	}

	if (!PackagingSettings->IncludeDebugFiles)
	{
		OptionalParams += TEXT(" -nodebuginfo");
	}


	bool bTargetPlatformCanUseCrashReporter = true;
	if (PlatformInfo->TargetPlatformName == FName("WindowsNoEditor") && PlatformInfo->PlatformFlavor == TEXT("Win32"))
	{
		FString MinumumSupportedWindowsOS;
		GConfig->GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("MinimumOSVersion"), MinumumSupportedWindowsOS, GEngineIni);
		if (MinumumSupportedWindowsOS == TEXT("MSOS_XP"))
		{
			OptionalParams += TEXT(" -SpecifiedArchitecture=_xp");
			bTargetPlatformCanUseCrashReporter = false;
		}
	}

	// Append any extra UAT flags specified for this platform flavor
	if (!PlatformInfo->UATCommandLine.IsEmpty())
	{
		OptionalParams += TEXT(" ");
		OptionalParams += PlatformInfo->UATCommandLine;
	}
	else
	{
		OptionalParams += TEXT(" -targetplatform=");
		OptionalParams += *PlatformInfo->TargetPlatformName.ToString();
	}

	// only build if the project has code that might need to be built
	if (bProjectHasCode && FSourceCodeNavigation::IsCompilerAvailable())
	{
		OptionalParams += TEXT(" -build");
	}

	// we can include the crash reporter for any non-code projects or internal builds (we don't have the symbols to interpret external builds of code-based projects)
	if (PackagingSettings->IncludeCrashReporter && (!bProjectHasCode || FEngineBuildSettings::IsInternalBuild()) && bTargetPlatformCanUseCrashReporter)
	{
		OptionalParams += TEXT(" -CrashReporter");
	}

	if (PackagingSettings->bBuildHttpChunkInstallData)
	{
		OptionalParams += FString::Printf(TEXT(" -manifests -createchunkinstall -chunkinstalldirectory=\"%s\" -chunkinstallversion=%s"), *(PackagingSettings->HttpChunkInstallDataDirectory.Path), *(PackagingSettings->HttpChunkInstallDataVersion));
	}

	FString ExecutableName = FPlatformProcess::ExecutableName(false);
#if PLATFORM_WINDOWS
	// turn UE4editor into UE4editor.cmd
	if(ExecutableName.EndsWith(".exe", ESearchCase::IgnoreCase) && !FPaths::GetBaseFilename(ExecutableName).EndsWith("-cmd", ESearchCase::IgnoreCase))
	{
		FString NewExeName = ExecutableName.Left(ExecutableName.Len() - 4) + "-Cmd.exe";
		if (FPaths::FileExists(NewExeName))
		{
			ExecutableName = NewExeName;
		}
	}
#endif

	const bool bRunningDebug = FParse::Param(FCommandLine::Get(), TEXT("debug"));

	if (bRunningDebug)
	{
		OptionalParams += TEXT(" -UseDebugParamForEditorExe");
	}

	FString Configuration = FindObject<UEnum>(ANY_PACKAGE, TEXT("EProjectPackagingBuildConfigurations"))->GetEnumName(PackagingSettings->BuildConfiguration);
	Configuration = Configuration.Replace(TEXT("PPBC_"), TEXT(""));

	FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()) : FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
	FString CommandLine = FString::Printf(TEXT("BuildCookRun %s%s%s -nop4 -project=\"%s\" -cook -stage -archive -archivedirectory=\"%s\" -package -clientconfig=%s -ue4exe=%s %s -utf8output"),
		FRocketSupport::IsRocket() ? TEXT("-rocket ") : TEXT(""),
		GetUATCompilationFlags(),
		FApp::IsEngineInstalled() ? TEXT(" -installed") : TEXT(""),
		*ProjectPath,
		*PackagingSettings->StagingDirectory.Path,
		*Configuration,
		*ExecutableName,
		*OptionalParams
	);

	CreateUatTask(CommandLine, PlatformInfo->DisplayName, LOCTEXT("PackagingProjectTaskName", "Packaging project"), LOCTEXT("PackagingTaskName", "Packaging"), FEditorStyle::GetBrush(TEXT("MainFrame.PackageProject")));
}

bool FMainFrameActionCallbacks::PackageProjectCanExecute( const FName PlatformInfoName )
{
	return true;
}

void FMainFrameActionCallbacks::RefreshCodeProject()
{
	if ( !FSourceCodeNavigation::IsCompilerAvailable() )
	{
		// Attempt to trigger the tutorial if the user doesn't have a compiler installed for the project.
		FSourceCodeNavigation::AccessOnCompilerNotFound().Broadcast();
	}

	FText FailReason, FailLog;
	if(!FGameProjectGenerationModule::Get().UpdateCodeProject(FailReason, FailLog))
	{
		SOutputLogDialog::Open(LOCTEXT("RefreshProject", "Refresh Project"), FailReason, FailLog, FText::GetEmpty());
	}
}

void FMainFrameActionCallbacks::OpenIDE()
{
	if ( !FSourceCodeNavigation::IsCompilerAvailable() )
	{
		// Attempt to trigger the tutorial if the user doesn't have a compiler installed for the project.
		FSourceCodeNavigation::AccessOnCompilerNotFound().Broadcast();
	}
	else
	{
		if ( !FSourceCodeNavigation::OpenModuleSolution() )
		{
			FString SolutionPath;
			if(FDesktopPlatformModule::Get()->GetSolutionPath(SolutionPath))
			{
				const FString FullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead( *SolutionPath );
				const FText Message = FText::Format( LOCTEXT( "OpenIDEFailed_MissingFile", "Could not open {0} for project {1}" ), FSourceCodeNavigation::GetSuggestedSourceCodeIDE(), FText::FromString( FullPath ) );
				FMessageDialog::Open( EAppMsgType::Ok, Message );
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("OpenIDEFailed_MissingSolution", "Couldn't find solution"));
			}
		}
	}
}

void FMainFrameActionCallbacks::PackagingSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Project", "Project", "Packaging");
}

//void FMainFrameActionCallbacks::LocalizeProject()
//{
//	FModuleManager::LoadModuleChecked<ILocalizationDashboardModule>("LocalizationDashboard").Show();
//}

void FMainFrameActionCallbacks::SwitchProjectByIndex( int32 ProjectIndex )
{
	FUnrealEdMisc::Get().SwitchProject( ProjectNames[ ProjectIndex ] );
}

void FMainFrameActionCallbacks::SwitchProject(const FString& GameOrProjectFileName)
{
	FUnrealEdMisc::Get().SwitchProject( GameOrProjectFileName );
}

void FMainFrameActionCallbacks::OpenBackupDirectory( FString BackupFile )
{
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*FPaths::GetPath(FPaths::ConvertRelativePathToFull(BackupFile)));
}

void FMainFrameActionCallbacks::ResetLayout()
{
	if(EAppReturnType::Ok != OpenMsgDlgInt(EAppMsgType::OkCancel, LOCTEXT( "ActionRestartMsg", "This action requires the editor to restart; you will be prompted to save any changes. Continue?" ), LOCTEXT( "ResetUILayout_Title", "Reset UI Layout" ) ) )
	{
		return;
	}

	// make a backup
	GetMutableDefault<UEditorPerProjectUserSettings>()->SaveConfig();

	FString BackupEditorLayoutIni = FString::Printf(TEXT("%s_Backup.ini"), *FPaths::GetBaseFilename(GEditorLayoutIni, false));

	if( COPY_Fail == IFileManager::Get().Copy(*BackupEditorLayoutIni, *GEditorLayoutIni) )
	{
		FMessageLog EditorErrors("EditorErrors");
		if(!FPaths::FileExists(GEditorLayoutIni))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(GEditorLayoutIni));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_NoExist_Notification", "Unsuccessful backup! {FileName} does not exist!"), Arguments));
		}
		else if(IFileManager::Get().IsReadOnly(*BackupEditorLayoutIni))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(FPaths::ConvertRelativePathToFull(BackupEditorLayoutIni)));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_ReadOnly_Notification", "Unsuccessful backup! {FileName} is read-only!"), Arguments));
		}
		else
		{
			// We don't specifically know why it failed, this is a fallback.
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("SourceFileName"), FText::FromString(GEditorLayoutIni));
			Arguments.Add(TEXT("BackupFileName"), FText::FromString(FPaths::ConvertRelativePathToFull(BackupEditorLayoutIni)));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_Fallback_Notification", "Unsuccessful backup of {SourceFileName} to {BackupFileName}"), Arguments));
		}
		EditorErrors.Notify(LOCTEXT("BackupUnsuccessful_Title", "Backup Unsuccessful!"));
	}
	else
	{
		FNotificationInfo ErrorNotification( FText::GetEmpty() );
		ErrorNotification.bFireAndForget = true;
		ErrorNotification.ExpireDuration = 3.0f;
		ErrorNotification.bUseThrobber = true;
		ErrorNotification.Hyperlink = FSimpleDelegate::CreateStatic(&FMainFrameActionCallbacks::OpenBackupDirectory, BackupEditorLayoutIni);
		ErrorNotification.HyperlinkText = LOCTEXT("SuccessfulBackup_Notification_Hyperlink", "Open Directory");
		ErrorNotification.Text = LOCTEXT("SuccessfulBackup_Notification", "Backup Successful!");
		ErrorNotification.Image = FEditorStyle::GetBrush(TEXT("NotificationList.SuccessImage"));
		FSlateNotificationManager::Get().AddNotification(ErrorNotification);
	}

	// reset layout & restart Editor
	FUnrealEdMisc::Get().AllowSavingLayoutOnClose(false);
	FUnrealEdMisc::Get().RestartEditor(false);
}

void FMainFrameActionCallbacks::SaveLayout()
{
	FGlobalTabmanager::Get()->SaveAllVisualState();

	// Write the saved state's config to disk
	GConfig->Flush( false, GEditorLayoutIni );
}

void FMainFrameActionCallbacks::ToggleFullscreen_Execute()
{
	if ( GIsEditor && FApp::HasGameName() )
	{
		static TWeakPtr<SDockTab> LevelEditorTabPtr = FGlobalTabmanager::Get()->InvokeTab(FTabId("LevelEditor"));
		const TSharedPtr<SWindow> LevelEditorWindow = FSlateApplication::Get().FindWidgetWindow( LevelEditorTabPtr.Pin().ToSharedRef() );

		if (LevelEditorWindow->GetWindowMode() == EWindowMode::Windowed)
		{
			LevelEditorWindow->SetWindowMode(EWindowMode::WindowedFullscreen);
		}
		else
		{
			LevelEditorWindow->SetWindowMode(EWindowMode::Windowed);
		}
	}	
}

bool FMainFrameActionCallbacks::FullScreen_IsChecked()
{
	const TSharedPtr<SDockTab> LevelEditorTabPtr = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>( "LevelEditor" ).GetLevelEditorTab();

	const TSharedPtr<SWindow> LevelEditorWindow = LevelEditorTabPtr.IsValid()
		? LevelEditorTabPtr->GetParentWindow()
		: TSharedPtr<SWindow>();

	return (LevelEditorWindow.IsValid())
		? (LevelEditorWindow->GetWindowMode() != EWindowMode::Windowed)
		: false;
}


bool FMainFrameActionCallbacks::CanSwitchToProject( int32 InProjectIndex )
{
	if (FApp::HasGameName() && ProjectNames[InProjectIndex].StartsWith(FApp::GetGameName()))
	{
		return false;
	}

	if ( FPaths::IsProjectFilePathSet() && ProjectNames[ InProjectIndex ] == FPaths::GetProjectFilePath() )
	{
		return false;
	}

	return true;
}

bool FMainFrameActionCallbacks::IsSwitchProjectChecked( int32 InProjectIndex )
{
	return CanSwitchToProject( InProjectIndex ) == false;
}


void FMainFrameActionCallbacks::Exit()
{
	FSlateApplication::Get().LeaveDebuggingMode();
	// Shut down the editor
	// NOTE: We can't close the editor from within this stack frame as it will cause various DLLs
	//       (such as MainFrame) to become unloaded out from underneath the code pointer.  We'll shut down
	//       as soon as it's safe to do so.
	GEngine->DeferredCommands.Add( TEXT("CLOSE_SLATE_MAINFRAME"));
}


bool FMainFrameActionCallbacks::Undo_CanExecute()
{
	return GUnrealEd->Trans->CanUndo() && FSlateApplication::Get().IsNormalExecution();
}

bool FMainFrameActionCallbacks::Redo_CanExecute()
{
	return GUnrealEd->Trans->CanRedo() && FSlateApplication::Get().IsNormalExecution();
}


void FMainFrameActionCallbacks::ExecuteExecCommand( FString Command )
{
	GUnrealEd->Exec( GEditor->GetEditorWorldContext(true).World(), *Command );
}


void FMainFrameActionCallbacks::OpenSlateApp_ViaModule( FName AppName, FName ModuleName )
{
	FModuleManager::Get().LoadModule( ModuleName );
	OpenSlateApp( AppName );
}

void FMainFrameActionCallbacks::OpenSlateApp( FName AppName )
{
	FGlobalTabmanager::Get()->InvokeTab(FTabId(AppName));
}

bool FMainFrameActionCallbacks::OpenSlateApp_IsChecked( FName AppName )
{
	return false;
}

void FMainFrameActionCallbacks::VisitAskAQuestionPage()
{
	FString AskAQuestionURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("AskAQuestionURL"), AskAQuestionURL, true ))
	{
		FPlatformProcess::LaunchURL( *AskAQuestionURL, NULL, NULL );
	}
}


void FMainFrameActionCallbacks::VisitSearchForAnswersPage()
{
	FString SearchForAnswersURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("SearchForAnswersURL"), SearchForAnswersURL, true ))
	{
		FPlatformProcess::LaunchURL( *SearchForAnswersURL, NULL, NULL );
	}
}


void FMainFrameActionCallbacks::VisitSupportWebSite()
{
	FString SupportWebsiteURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("SupportWebsiteURL"), SupportWebsiteURL, true ))
	{
		FPlatformProcess::LaunchURL( *SupportWebsiteURL, NULL, NULL );
	}
}


void FMainFrameActionCallbacks::VisitEpicGamesDotCom()
{
	FString EpicGamesURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("EpicGamesURL"), EpicGamesURL ))
	{
		FPlatformProcess::LaunchURL( *EpicGamesURL, NULL, NULL );
	}
}

void FMainFrameActionCallbacks::VisitWiki()
{
	FString URL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("WikiURL"), URL))
	{
		FPlatformProcess::LaunchURL(*URL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::VisitForums()
{
	FString URL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("ForumsURL"), URL))
	{
		FPlatformProcess::LaunchURL(*URL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::AboutUnrealEd_Execute()
{
	const FText AboutWindowTitle = LOCTEXT( "AboutUnrealEditor", "About Unreal Editor" );

	TSharedPtr<SWindow> AboutWindow = 
		SNew(SWindow)
		.Title( AboutWindowTitle )
		.ClientSize(FVector2D(600.f, 200.f))
		.SupportsMaximize(false) .SupportsMinimize(false)
		.SizingRule( ESizingRule::FixedSize )
		[
			SNew(SAboutScreen)
		];

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
	TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

	if ( ParentWindow.IsValid() )
	{
		FSlateApplication::Get().AddModalWindow(AboutWindow.ToSharedRef(), ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(AboutWindow.ToSharedRef());
	}
}

void FMainFrameActionCallbacks::CreditsUnrealEd_Execute()
{
	const FText CreditsWindowTitle = LOCTEXT("CreditsUnrealEditor", "Credits");

	TSharedPtr<SWindow> CreditsWindow =
		SNew(SWindow)
		.Title(CreditsWindowTitle)
		.ClientSize(FVector2D(600.f, 700.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		[
			SNew(SCreditsScreen)
		];

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

	if ( ParentWindow.IsValid() )
	{
		FSlateApplication::Get().AddModalWindow(CreditsWindow.ToSharedRef(), ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(CreditsWindow.ToSharedRef());
	}
}

void FMainFrameActionCallbacks::OpenWidgetReflector_Execute()
{
	FGlobalTabmanager::Get()->InvokeTab(FTabId("WidgetReflector"));
}


/* FMainFrameActionCallbacks implementation
 *****************************************************************************/

void FMainFrameActionCallbacks::AddMessageLog( const FText& Text, const FText& Detail, const FString& TutorialLink )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(Text));
	Message->AddToken(FTextToken::Create(Detail));
	Message->AddToken(FTutorialToken::Create(TutorialLink));
	Message->AddToken(FDocumentationToken::Create(TEXT("Platforms/iOS/QuickStart/6")));

	FMessageLog MessageLog("PackagingResults");
	MessageLog.AddMessage(Message);
	MessageLog.Open();
}


void FMainFrameActionCallbacks::CreateUatTask( const FString& CommandLine, const FText& PlatformDisplayName, const FText& TaskName, const FText &TaskShortName, const FSlateBrush* TaskIcon )
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
	FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
	bool bHasCode = GameProjectModule.Get().ProjectHasCodeFiles();

	if (!FPaths::FileExists(UatPath))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("File"), FText::FromString(UatPath));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiredFileNotFoundMessage", "A required file could not be found:\n{File}"), Arguments));

		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), 0.0));
		FString EventName = (CommandLine.Contains(TEXT("-package")) ? TEXT("Editor.Package") : TEXT("Editor.Cook"));
		FEditorAnalytics::ReportEvent(EventName + TEXT(".Failed"), PlatformDisplayName.ToString(), bHasCode, EAnalyticsErrorCodes::UATNotFound, ParamArray);
		
		return;
	}

#if PLATFORM_WINDOWS
	FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *UatPath, *CommandLine);
#else
	FString FullCommandLine = FString::Printf(TEXT("\"%s\" %s"), *UatPath, *CommandLine);
#endif

	TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));

	// create notification item
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Platform"), PlatformDisplayName);
	Arguments.Add(TEXT("TaskName"), TaskName);
	FNotificationInfo Info( FText::Format( LOCTEXT("UatTaskInProgressNotification", "{TaskName} for {Platform}..."), Arguments) );
	
	Info.Image = TaskIcon;
	Info.bFireAndForget = false;
	Info.ExpireDuration = 3.0f;
	Info.Hyperlink = FSimpleDelegate::CreateStatic(&FMainFrameActionCallbacks::HandleUatHyperlinkNavigate);
	Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
	Info.ButtonDetails.Add(
		FNotificationButtonInfo(
			LOCTEXT("UatTaskCancel", "Cancel"),
			LOCTEXT("UatTaskCancelToolTip", "Cancels execution of this task."),
			FSimpleDelegate::CreateStatic(&FMainFrameActionCallbacks::HandleUatCancelButtonClicked, UatProcess)
		)
	);

	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

	if (!NotificationItem.IsValid())
	{
		return;
	}

	FString EventName = (CommandLine.Contains(TEXT("-package")) ? TEXT("Editor.Package") : TEXT("Editor.Cook"));
	FEditorAnalytics::ReportEvent(EventName + TEXT(".Start"), PlatformDisplayName.ToString(), bHasCode);

	NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);

	// launch the packager
	TWeakPtr<SNotificationItem> NotificationItemPtr(NotificationItem);

	EventData Data;
	Data.StartTime = FPlatformTime::Seconds();
	Data.EventName = EventName;
	Data.bProjectHasCode = bHasCode;
	UatProcess->OnCanceled().BindStatic(&FMainFrameActionCallbacks::HandleUatProcessCanceled, NotificationItemPtr, PlatformDisplayName, TaskShortName, Data);
	UatProcess->OnCompleted().BindStatic(&FMainFrameActionCallbacks::HandleUatProcessCompleted, NotificationItemPtr, PlatformDisplayName, TaskShortName, Data);
	UatProcess->OnOutput().BindStatic(&FMainFrameActionCallbacks::HandleUatProcessOutput, NotificationItemPtr, PlatformDisplayName, TaskShortName);

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

		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), 0.0));
		FEditorAnalytics::ReportEvent(EventName + TEXT(".Failed"), PlatformDisplayName.ToString(), bHasCode, EAnalyticsErrorCodes::UATLaunchFailure, ParamArray);
	}
}

/* FMainFrameActionCallbacks callbacks
 *****************************************************************************/

class FMainFrameActionsNotificationTask
{
public:

	FMainFrameActionsNotificationTask( TWeakPtr<SNotificationItem> InNotificationItemPtr, SNotificationItem::ECompletionState InCompletionState, const FText& InText )
		: CompletionState(InCompletionState)
		, NotificationItemPtr(InNotificationItemPtr)
		, Text(InText)
	{ }

	void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent )
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
	ENamedThreads::Type GetDesiredThread( ) { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMainFrameActionsNotificationTask, STATGROUP_TaskGraphTasks);
	}

private:

	SNotificationItem::ECompletionState CompletionState;
	TWeakPtr<SNotificationItem> NotificationItemPtr;
	FText Text;
};


void FMainFrameActionCallbacks::HandleUatHyperlinkNavigate( )
{
	FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog"));
}


void FMainFrameActionCallbacks::HandleUatCancelButtonClicked( TSharedPtr<FMonitoredProcess> PackagerProcess )
{
	if (PackagerProcess.IsValid())
	{
		PackagerProcess->Cancel(true);
	}
}


void FMainFrameActionCallbacks::HandleUatProcessCanceled( TWeakPtr<SNotificationItem> NotificationItemPtr, FText PlatformDisplayName, FText TaskName, EventData Event )
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Platform"), PlatformDisplayName);
	Arguments.Add(TEXT("TaskName"), TaskName);

	TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
		NotificationItemPtr,
		SNotificationItem::CS_Fail,
		FText::Format(LOCTEXT("UatProcessFailedNotification", "{TaskName} canceled!"), Arguments)
	);

	TArray<FAnalyticsEventAttribute> ParamArray;
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), FPlatformTime::Seconds() - Event.StartTime));
	FEditorAnalytics::ReportEvent(Event.EventName + TEXT(".Canceled"), PlatformDisplayName.ToString(), Event.bProjectHasCode, ParamArray);
//	FMessageLog("PackagingResults").Warning(FText::Format(LOCTEXT("UatProcessCanceledMessageLog", "{TaskName} for {Platform} canceled by user"), Arguments));
}


DECLARE_CYCLE_STAT(TEXT("Requesting FMainFrameActionCallbacks::HandleUatProcessCompleted message dialog to present the error message"), STAT_FMainFrameActionCallbacks_HandleUatProcessCompleted_DialogMessage, STATGROUP_TaskGraphTasks);
void FMainFrameActionCallbacks::HandleUatProcessCompleted( int32 ReturnCode, TWeakPtr<SNotificationItem> NotificationItemPtr, FText PlatformDisplayName, FText TaskName, EventData Event )
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Platform"), PlatformDisplayName);
	Arguments.Add(TEXT("TaskName"), TaskName);

	if (ReturnCode == 0)
	{
		TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Success,
			FText::Format(LOCTEXT("UatProcessSucceededNotification", "{TaskName} complete!"), Arguments)
		);

		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), FPlatformTime::Seconds() - Event.StartTime));
		FEditorAnalytics::ReportEvent(Event.EventName + TEXT(".Completed"), PlatformDisplayName.ToString(), Event.bProjectHasCode, ParamArray);

//		FMessageLog("PackagingResults").Info(FText::Format(LOCTEXT("UatProcessSuccessMessageLog", "{TaskName} for {Platform} completed successfully"), Arguments));
	}
	else
	{
		TGraphTask<FMainFrameActionsNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Fail,
			FText::Format(LOCTEXT("PackagerFailedNotification", "{TaskName} failed!"), Arguments)
		);

		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), FPlatformTime::Seconds() - Event.StartTime));
		FEditorAnalytics::ReportEvent(Event.EventName + TEXT(".Failed"), PlatformDisplayName.ToString(), Event.bProjectHasCode, ReturnCode, ParamArray);

		// Present a message dialog if we want the error message to be prominent.
		if (FEditorAnalytics::ShouldElevateMessageThroughDialog(ReturnCode))
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FEditorAnalytics::TranslateErrorCode(ReturnCode)));
				}),
				GET_STATID(STAT_FMainFrameActionCallbacks_HandleUatProcessCompleted_DialogMessage),
				nullptr,
				ENamedThreads::GameThread
			);
		}
//		FMessageLog("PackagingResults").Info(FText::Format(LOCTEXT("UatProcessFailedMessageLog", "{TaskName} for {Platform} failed"), Arguments));
	}
}


void FMainFrameActionCallbacks::HandleUatProcessOutput( FString Output, TWeakPtr<SNotificationItem> NotificationItemPtr, FText PlatformDisplayName, FText TaskName )
{
	if (!Output.IsEmpty() && !Output.Equals("\r"))
	{
		UE_LOG(MainFrameActions, Log, TEXT("%s (%s): %s"), *TaskName.ToString(), *PlatformDisplayName.ToString(), *Output);
	}	
}


#undef LOCTEXT_NAMESPACE
