// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ShmMessagingPrivatePCH.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"


#define LOCTEXT_NAMESPACE "FShmMessagingModule"


/**
 * Implements the ShmMessagingModule module.
 */
class FShmMessagingModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		if (!SupportsSharedMemTransport())
		{
			return;
		}

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "ShmMessaging",
				LOCTEXT("ShmMessagingSettingsName", "SharedMem Messaging"),
				LOCTEXT("ShmMessagingSettingsDescription", "Configure the Shared Memory Messaging plug-in."),
				GetMutableDefault<UShmMessagingSettings>()
			);

			if (SettingsSection.IsValid())
			{
				SettingsSection->OnModified().BindRaw(this, &FShmMessagingModule::HandleSettingsSaved);
			}
		}

		// register application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FShmMessagingModule::HandleApplicationHasReactivated);
		FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FShmMessagingModule::HandleApplicationWillDeactivate);

		RestartServices();
	}

	virtual void ShutdownModule() override
	{
		// unregister application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);

		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "ShmMessaging");
		}

		// shut down services
		ShutdownBridge();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

protected:

	/** Initializes the message bridge with the current settings. */
	void InitializeBridge()
	{
		ShutdownBridge();

		UShmMessagingSettings* Settings = GetMutableDefault<UShmMessagingSettings>();
		bool ResaveSettings = false;


		if (ResaveSettings)
		{
			Settings->SaveConfig();
		}

		GLog->Logf(TEXT("ShmMessaging: Initializing bridge."));

		MessageBridge = FMessageBridgeBuilder()
			.UsingTransport(MakeShareable(new FShmMessageTransport()));
	}


	/** Restarts the bridge and tunnel services. */
	void RestartServices()
	{
		const UShmMessagingSettings& Settings = *GetDefault<UShmMessagingSettings>();

		if (Settings.EnableTransport)
		{
			if (!MessageBridge.IsValid())
			{
				InitializeBridge();
			}
		}
		else
		{
			ShutdownBridge();
		}
	}

	/**
	 * Checks whether networked message transport is supported.
	 *
	 * @todo gmp: this should be moved into an Engine module, so it can be shared with other transports
	 * @return true if networked transport is supported, false otherwise.
	 */
	bool SupportsSharedMemTransport() const
	{
		if (FApp::IsGame())
		{
			// only allow in Debug and Development configurations for now
			if ((FApp::GetBuildConfiguration() == EBuildConfigurations::Shipping) || (FApp::GetBuildConfiguration() == EBuildConfigurations::Test))
			{
				return false;
			}

			// disallow unsupported platforms
			if (!FPlatformMisc::SupportsMessaging() || !FPlatformProcess::SupportsMultithreading())
			{
				return false;
			}
		}

		if (FApp::IsGame() || IsRunningCommandlet())
		{
			// only allow UDP transport if explicitly desired
			if (!FParse::Param(FCommandLine::Get(), TEXT("Messaging")))
			{
				return false;
			}
		}

		return true;
	}

	/** Shuts down the message bridge. */
	void ShutdownBridge()
	{
		if (MessageBridge.IsValid())
		{
			MessageBridge->Disable();
			FPlatformProcess::Sleep(0.1f);
			MessageBridge.Reset();
		}
	}

private:

	/** Callback for when an has been reactivated (i.e. return from sleep on iOS). */
	void HandleApplicationHasReactivated()
	{
		RestartServices();
	}

	/** Callback for when the application will be deactivated (i.e. sleep on iOS).*/
	void HandleApplicationWillDeactivate()
	{
		ShutdownBridge();
	}

	/** Callback for when the settings were saved. */
	bool HandleSettingsSaved()
	{
		RestartServices();

		return true;
	}

private:

	/** Holds the message bridge if present. */
	IMessageBridgePtr MessageBridge;
};


IMPLEMENT_MODULE(FShmMessagingModule, ShmMessaging);


#undef LOCTEXT_NAMESPACE
