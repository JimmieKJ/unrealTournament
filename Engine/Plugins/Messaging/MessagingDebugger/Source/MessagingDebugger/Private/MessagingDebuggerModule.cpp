// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingDebuggerPrivatePCH.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "SDockTab.h"


#define LOCTEXT_NAMESPACE "FMessagingDebuggerModule"


static const FName MessagingDebuggerTabName("MessagingDebugger");


/**
 * Implements the MessagingDebugger module.
 */
class FMessagingDebuggerModule
	: public IModuleInterface
	, public IModularFeature
{
public:

	// IModuleInterface interface
	
	virtual void StartupModule() override
	{
		Style = MakeShareable(new FMessagingDebuggerStyle());

		FMessagingDebuggerCommands::Register();

		IModularFeatures::Get().RegisterModularFeature("MessagingDebugger", this);
		
		// This is still experimental in the editor, so it'll be invoked specifically in FMainMenu if the experimental settings flag is set.
		// When no longer experimental, switch to the nomad spawner registration below
		FGlobalTabmanager::Get()->RegisterTabSpawner(MessagingDebuggerTabName, FOnSpawnTab::CreateRaw(this, &FMessagingDebuggerModule::SpawnMessagingDebuggerTab));
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner(MessagingDebuggerTabName);

		IModularFeatures::Get().UnregisterModularFeature("MessagingDebugger", this);
		FMessagingDebuggerCommands::Unregister();
	}

private:

	/**
	 * Creates a new messaging debugger tab.
	 *
	 * @param SpawnTabArgs The arguments for the tab to spawn.
	 * @return The spawned tab.
	 */
	TSharedRef<SDockTab> SpawnMessagingDebuggerTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		const TSharedRef<SDockTab> MajorTab = SNew(SDockTab)
			.TabRole(ETabRole::MajorTab);

		TSharedPtr<SWidget> TabContent;
		IMessageBusPtr MessageBus = IMessagingModule::Get().GetDefaultBus();

		if (MessageBus.IsValid())
		{
			TabContent = SNew(SMessagingDebugger, MajorTab, SpawnTabArgs.GetOwnerWindow(), MessageBus->GetTracer(), Style.ToSharedRef());
		}
		else
		{
			TabContent = SNew(STextBlock)
				.Text(LOCTEXT("MessagingSystemUnavailableError", "Sorry, the Messaging system is not available right now"));
		}

		MajorTab->SetContent(TabContent.ToSharedRef());

		return MajorTab;
	}

private:

	/** Holds the plug-ins style set. */
	TSharedPtr<ISlateStyle> Style;
};


IMPLEMENT_MODULE(FMessagingDebuggerModule, MessagingDebugger);


#undef LOCTEXT_NAMESPACE