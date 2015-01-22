// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessageLogPrivatePCH.h"
#include "MessageLogModule.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "TokenizedMessage.h"
#include "SDockTab.h"
#include "SMessageLog.h"
#include "SMessageLogListing.h"
#include "MessageLogModel.h"
#include "MessageLogViewModel.h"
#include "MessageLogListingModel.h"
#include "MessageLogListingViewModel.h"
#include "MessageLog.h"

IMPLEMENT_MODULE( FMessageLogModule, MessageLog );

TSharedRef<SDockTab> SpawnMessageLog( const FSpawnTabArgs& Args, TSharedRef<FMessageLogViewModel> MessageLogViewModel )
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SMessageLog, MessageLogViewModel)
		];
}

static TSharedRef<IMessageLog> GetLog(const FName& LogName)
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	return MessageLogModule.GetLogListing(LogName);
}

FMessageLogModule::FMessageLogModule()
	: bCanDisplayMessageLog(false)
{
}

void FMessageLogModule::StartupModule()
{
	MessageLogViewModel = MakeShareable(new FMessageLogViewModel(MakeShareable(new FMessageLogModel())));
	MessageLogViewModel->Initialize();

#if WITH_EDITOR
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("MessageLog", FOnSpawnTab::CreateStatic( &SpawnMessageLog, MessageLogViewModel.ToSharedRef() ))
		.SetDisplayName(NSLOCTEXT("UnrealEditor", "MessageLogTab", "Message Log"))
		.SetTooltipText(NSLOCTEXT("UnrealEditor", "MessageLogTooltipText", "Open the Message Log tab."))
		.SetGroup( WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory() )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "MessageLog.TabIcon"));
#endif

	// Bind us so message log output is routed via this module
	FMessageLog::OnGetLog().BindStatic(&GetLog);
}

void FMessageLogModule::ShutdownModule()
{
#if WITH_EDITOR
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("MessageLog");
	}
#endif

	FMessageLog::OnGetLog().Unbind();
}

TSharedRef<IMessageLogListing> FMessageLogModule::GetLogListing(const FName& LogName)
{
	return MessageLogViewModel->GetLogListingViewModel(LogName);
}

void FMessageLogModule::RegisterLogListing(const FName& LogName, const FText& LogLabel, const FMessageLogInitializationOptions& InitializationOptions)
{
	MessageLogViewModel->RegisterLogListingViewModel(LogName, LogLabel, InitializationOptions);
}

bool FMessageLogModule::UnregisterLogListing(const FName& LogName)
{
	return MessageLogViewModel->UnregisterLogListingViewModel(LogName);
}	

bool FMessageLogModule::IsRegisteredLogListing(const FName& LogName) const
{
	return MessageLogViewModel->IsRegisteredLogListingViewModel(LogName);
}

TSharedRef<IMessageLogListing> FMessageLogModule::CreateLogListing(const FName& InLogName, const FMessageLogInitializationOptions& InitializationOptions)
{
	TSharedRef<FMessageLogListingModel> MessageLogListingModel = FMessageLogListingModel::Create( InLogName );
	return FMessageLogListingViewModel::Create( MessageLogListingModel, FText(), InitializationOptions );
}

TSharedRef<SWidget> FMessageLogModule::CreateLogListingWidget(const TSharedRef<IMessageLogListing>& InMessageLogListing)
{
	return SNew(SMessageLogListing, InMessageLogListing);
}

void FMessageLogModule::OpenMessageLog(const FName& LogName)
{
	// only open the message log if we have a window created for the tab manager & our delegate allows it
	if(bCanDisplayMessageLog
#if !PLATFORM_MAC
	   && FGlobalTabmanager::Get()->GetRootWindow().IsValid()
#endif
	   )
	{
		FGlobalTabmanager::Get()->InvokeTab(FName("MessageLog"));
		MessageLogViewModel->ChangeCurrentListingViewModel(LogName);
	}
}

void FMessageLogModule::EnableMessageLogDisplay(bool bInCanDisplayMessageLog)
{
	bCanDisplayMessageLog = bInCanDisplayMessageLog;
}