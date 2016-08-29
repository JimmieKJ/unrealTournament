// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TcpMessagingPrivatePCH.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif
#include "ModuleManager.h"
#include "IConnectionBasedMessagingModule.h"


DEFINE_LOG_CATEGORY(LogTcpMessaging);

#define LOCTEXT_NAMESPACE "FTcpMessagingModule"


/**
 * Implements the TcpMessagingModule module.
 */
class FTcpMessagingModule
	: public FSelfRegisteringExec
	, public IConnectionBasedMessagingModule
{
public:

	// FSelfRegisteringExec interface

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (!FParse::Command(&Cmd, TEXT("TCPMESSAGING")))
		{
			return false;
		}

		if (FParse::Command(&Cmd, TEXT("STATUS")))
		{
			UTcpMessagingSettings* Settings = GetMutableDefault<UTcpMessagingSettings>();

			// general information
			Ar.Logf(TEXT("Protocol Version: %i"), TCP_MESSAGING_TRANSPORT_PROTOCOL_VERSION);

			// bridge status
			if (MessageBridge.IsValid())
			{
				if (MessageBridge->IsEnabled())
				{
					Ar.Log(TEXT("Message Bridge: Initialized and enabled"));
				}
				else
				{
					Ar.Log(TEXT("Message Bridge: Initialized, but disabled"));
				}
			}
			else
			{
				Ar.Log(TEXT("Message Bridge: Not initialized."));
			}
		}
		else if (FParse::Command(&Cmd, TEXT("RESTART")))
		{
			RestartServices();
		}
		else if (FParse::Command(&Cmd, TEXT("SHUTDOWN")))
		{
			ShutdownBridge();
		}
		else
		{
			// show usage
			Ar.Log(TEXT("Usage: TCPMESSAGING <Command>"));
			Ar.Log(TEXT(""));
			Ar.Log(TEXT("Command"));
			Ar.Log(TEXT("    RESTART = Restarts the message bridge, if enabled"));
			Ar.Log(TEXT("    SHUTDOWN = Shut down the message bridge, if running"));
			Ar.Log(TEXT("    STATUS = Displays the status of the TCP message transport"));
		}

		return true;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		if (!SupportsNetworkedTransport())
		{
			return;
		}

		// load dependencies
		if (!FModuleManager::Get().LoadModule(TEXT("Networking")).IsValid())
		{
			UE_LOG(LogTcpMessaging, Error, TEXT("The required module 'Networking' failed to load. Plug-in 'Tcp Messaging' cannot be used."));

			return;
		}

#if WITH_EDITOR
		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "TcpMessaging",
				LOCTEXT("TcpMessagingSettingsName", "TCP Messaging"),
				LOCTEXT("TcpMessagingSettingsDescription", "Configure the TCP Messaging plug-in."),
				GetMutableDefault<UTcpMessagingSettings>()
				);

			if (SettingsSection.IsValid())
			{
				SettingsSection->OnModified().BindRaw(this, &FTcpMessagingModule::HandleSettingsSaved);
			}
		}
#endif // WITH_EDITOR

		// register application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FTcpMessagingModule::HandleApplicationHasReactivated);
		FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FTcpMessagingModule::HandleApplicationWillDeactivate);

		RestartServices();
	}

	virtual void ShutdownModule() override
	{
		// unregister application events
		FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);

#if WITH_EDITOR
		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "TcpMessaging");
		}
#endif

		// shut down services
		ShutdownBridge();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

	virtual void AddOutgoingConnection(const FString& EndpointString) override
	{
		FIPv4Endpoint Endpoint;
		if (FIPv4Endpoint::Parse(EndpointString, Endpoint))
		{
			auto Transport = MessageTransportPtr.Pin();
			if (Transport.IsValid())
			{
				Transport->AddOutgoingConnection(Endpoint);
			}
		}
	}

	virtual void RemoveOutgoingConnection(const FString& EndpointString) override
	{
		FIPv4Endpoint Endpoint;
		if (FIPv4Endpoint::Parse(EndpointString, Endpoint))
		{
			auto Transport = MessageTransportPtr.Pin();
			if (Transport.IsValid())
			{
				Transport->RemoveOutgoingConnection(Endpoint);
			}
		}
	}

protected:

	/** Initializes the message bridge with the current settings. */
	void InitializeBridge()
	{
		ShutdownBridge();

		const UTcpMessagingSettings* Settings = GetDefault<UTcpMessagingSettings>();
		bool ResaveSettings = false;

		FIPv4Endpoint ListenEndpoint;
		TArray<FIPv4Endpoint> ConnectToEndpoints;

		if (!FIPv4Endpoint::Parse(Settings->ListenEndpoint, ListenEndpoint))
		{
			if (!Settings->ListenEndpoint.IsEmpty())
			{
				UE_LOG(LogTcpMessaging, Warning, TEXT("Invalid setting for ListenEndpoint '%s', listening disabled"), *Settings->ListenEndpoint);
			}

			ListenEndpoint = FIPv4Endpoint::Any;
		}
		
		for (auto& ConnectToEndpointString : Settings->ConnectToEndpoints)
		{
			FIPv4Endpoint ConnectToEndpoint;
			if (FIPv4Endpoint::Parse(ConnectToEndpointString, ConnectToEndpoint) )
			{
				ConnectToEndpoints.Add(ConnectToEndpoint);
			}
			else
			{
				UE_LOG(LogTcpMessaging, Warning, TEXT("Invalid entry for ConnectToEndpoint '%s', ignoring"), *ConnectToEndpointString);
			}
		}

		FString Status(TEXT("Initializing TcpMessaging bridge"));
		if (ConnectToEndpoints.Num())
		{
			Status += FString::Printf(TEXT(" for %d outgoing connections"), ConnectToEndpoints.Num());
		}
		if (ListenEndpoint != FIPv4Endpoint::Any)
		{
			Status += TEXT(", listening on ");
			Status += ListenEndpoint.ToText().ToString();
		}

		UE_LOG(LogTcpMessaging, Log, TEXT("%s"), *Status);

		TSharedRef<FTcpMessageTransport, ESPMode::ThreadSafe> Transport = MakeShareable(new FTcpMessageTransport(ListenEndpoint, ConnectToEndpoints, Settings->ConnectionRetryDelay));
		
		// Safe weak pointer for adding/removing connections
		MessageTransportPtr = Transport;

		MessageBridge = FMessageBridgeBuilder().UsingTransport(Transport);
	}

	/** Restarts the bridge service. */
	void RestartServices()
	{
		const UTcpMessagingSettings* Settings = GetDefault<UTcpMessagingSettings>();

		if (Settings->EnableTransport)
		{
			InitializeBridge();
		}
		else
		{
			ShutdownBridge();
		}
	}

	/**
	 * Checks whether networked message transport is supported.
	 *
	 * @return true if networked transport is supported, false otherwise.
	 */
	bool SupportsNetworkedTransport() const
	{
		// disallow unsupported platforms
		if (!FPlatformMisc::SupportsMessaging())
		{
			return false;
		}

		// single thread support not implemented yet
		if (!FPlatformProcess::SupportsMultithreading())
		{
			return false;
		}

		// always allow in standalone Slate applications
		if (!FApp::IsGame() && !IsRunningCommandlet())
		{
			return true;
		}

		// otherwise only allow if explicitly desired
		return FParse::Param(FCommandLine::Get(), TEXT("Messaging"));
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
	
	/** Message transport pointer, if valid */
	TWeakPtr<FTcpMessageTransport, ESPMode::ThreadSafe> MessageTransportPtr;
};

IMPLEMENT_MODULE(FTcpMessagingModule, TcpMessaging);

/** UTcpMessagingSettings constructor */
UTcpMessagingSettings::UTcpMessagingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// default values are taken from the ini configuration
}

#undef LOCTEXT_NAMESPACE
