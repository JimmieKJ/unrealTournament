// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingPrivatePCH.h"
#include "IMessagingModule.h"
#include "ModuleManager.h"


#ifndef PLATFORM_SUPPORTS_MESSAGEBUS
	#define PLATFORM_SUPPORTS_MESSAGEBUS 1
#endif


/**
 * Implements the Messaging module.
 */
class FMessagingModule
	: public FSelfRegisteringExec
	, public IMessagingModule
{
public:

	// FSelfRegisteringExec interface

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if (FParse::Command(&Cmd, TEXT("MESSAGING")))
		{
			if (FParse::Command(&Cmd, TEXT("STATUS")))
			{
				if (DefaultBus.IsValid())
				{
					Ar.Log(TEXT("Default message bus has been initialized."));
				}
				else
				{
					Ar.Log(TEXT("Default message bus has NOT been initialized yet."));
				}
			}
			else
			{
				// show usage
				Ar.Log(TEXT("Usage: MESSAGING <Command>"));
				Ar.Log(TEXT(""));
				Ar.Log(TEXT("Command"));
				Ar.Log(TEXT("    STATUS = Displays the status of the default message bus"));
			}

			return true;
		}

		return false;
	}

public:

	// IMessagingModule interface

	virtual IMessageBridgePtr CreateBridge( const FMessageAddress& Address, const IMessageBusRef& Bus, const IMessageTransportRef& Transport ) override
	{
		return MakeShareable(new FMessageBridge(Address, Bus, Transport));
	}

	virtual IMessageBusPtr CreateBus( const IAuthorizeMessageRecipientsPtr& RecipientAuthorizer ) override
	{
		return MakeShareable(new FMessageBus(RecipientAuthorizer));
	}

	virtual IMessageBusPtr GetDefaultBus() const override
	{
		return DefaultBus;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
#if PLATFORM_SUPPORTS_MESSAGEBUS
		FCoreDelegates::OnPreExit.AddRaw(this, &FMessagingModule::HandleCorePreExit);
		DefaultBus = CreateBus(nullptr);
#endif	//PLATFORM_SUPPORTS_MESSAGEBUS
	}

	virtual void ShutdownModule() override
	{
		ShutdownDefaultBus();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

protected:

	void ShutdownDefaultBus()
	{
		if (!DefaultBus.IsValid())
		{
			return;
		}

		IMessageBusWeakPtr DefaultBusPtr = DefaultBus;

		DefaultBus->Shutdown();
		DefaultBus.Reset();

		// wait for the bus to shut down
		int32 SleepCount = 0;

		while (DefaultBusPtr.IsValid())
		{
			check(SleepCount < 10);	// something is holding on to the message bus
			++SleepCount;

			FPlatformProcess::Sleep(0.1f);
		}
	}

private:

	// Callback for Core shutdown.
	void HandleCorePreExit()
	{
		ShutdownDefaultBus();
	}

private:

	// Holds the message bus.
	IMessageBusPtr DefaultBus;
};


/* Misc static functions
 *****************************************************************************/

TStatId FMessageDispatchTask::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FMessageDispatchTask, STATGROUP_TaskGraphTasks);
}


IMPLEMENT_MODULE(FMessagingModule, Messaging);
