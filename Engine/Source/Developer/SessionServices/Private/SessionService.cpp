// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SessionServicesPrivatePCH.h"


/* FSessionService structors
 *****************************************************************************/

FSessionService::FSessionService( const IMessageBusRef& InMessageBus )
	: MessageBusPtr(InMessageBus)
{ }


FSessionService::~FSessionService()
{
	Stop();
}


/* ISessionService interface
 *****************************************************************************/

bool FSessionService::Start()
{
	IMessageBusPtr MessageBus = MessageBusPtr.Pin();

	if (!MessageBus.IsValid())
	{
		return false;
	}

	// initialize messaging
	MessageEndpoint = FMessageEndpoint::Builder("FSessionService", MessageBus.ToSharedRef())
		.Handling<FSessionServiceLogSubscribe>(this, &FSessionService::HandleSessionLogSubscribeMessage)
		.Handling<FSessionServiceLogUnsubscribe>(this, &FSessionService::HandleSessionLogUnsubscribeMessage)
		.Handling<FSessionServicePing>(this, &FSessionService::HandleSessionPingMessage);

	if (!MessageEndpoint.IsValid())
	{
		return false;
	}

	MessageEndpoint->Subscribe<FSessionServicePing>();
	GLog->AddOutputDevice(this);

	return true;
}


void FSessionService::Stop()
{
	if (IsRunning())
	{
		GLog->RemoveOutputDevice(this);
		MessageEndpoint.Reset();
	}
}


/* FSessionService implementation
 *****************************************************************************/

void FSessionService::SendLog( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if (MessageEndpoint.IsValid())
	{
		FScopeLock Lock(&LogSubscribersLock);

		if (LogSubscribers.Num() > 0)
		{
			MessageEndpoint->Send(
				new FSessionServiceLog(
					Category,
					Data,
					FApp::GetInstanceId(),
					FPlatformTime::Seconds() - GStartTime,
					Verbosity
				),
				LogSubscribers
			);
		}
	}
}


void FSessionService::SendNotification( const TCHAR* NotificationText, const FMessageAddress& Recipient )
{
	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Send(
			new FSessionServiceLog(
				FName("RemoteSession"),
				NotificationText,
				FApp::GetInstanceId(),
				FPlatformTime::Seconds() - GStartTime,
				ELogVerbosity::Display
			),
			Recipient
		);
	}
}


void FSessionService::SendPong( const IMessageContextRef& Context )
{
	if (MessageEndpoint.IsValid())
	{
		FSessionServicePong* Message = new FSessionServicePong();

		Message->BuildDate = FApp::GetBuildDate();
		Message->DeviceName = FPlatformProcess::ComputerName();
		Message->InstanceId = FApp::GetInstanceId();
		Message->InstanceName = FApp::GetInstanceName();

#if PLATFORM_DESKTOP
		Message->IsConsoleBuild = false;
#else
		Message->IsConsoleBuild = true;
#endif

		Message->PlatformName = FPlatformProperties::PlatformName();
		Message->SessionId = FApp::GetSessionId();
		Message->SessionName = FApp::GetSessionName();
		Message->SessionOwner = FApp::GetSessionOwner();
		Message->Standalone = FApp::IsStandalone();

		MessageEndpoint->Send(Message, Context->GetSender());
	}
}


/* FSessionService callbacks
 *****************************************************************************/

void FSessionService::HandleMessageEndpointShutdown()
{
	MessageEndpoint.Reset();
}


void FSessionService::HandleSessionLogSubscribeMessage( const FSessionServiceLogSubscribe& Message, const IMessageContextRef& Context )
{
	FScopeLock Lock(&LogSubscribersLock);
	LogSubscribers.AddUnique(Context->GetSender());
}


void FSessionService::HandleSessionLogUnsubscribeMessage( const FSessionServiceLogUnsubscribe& Message, const IMessageContextRef& Context )
{
	FScopeLock Lock(&LogSubscribersLock);
	LogSubscribers.Remove(Context->GetSender());
}


void FSessionService::HandleSessionPingMessage( const FSessionServicePing& Message, const IMessageContextRef& Context )
{
	SendPong(Context);
}
