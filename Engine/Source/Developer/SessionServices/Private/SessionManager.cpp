// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SessionServicesPrivatePCH.h"


/* FSessionManager structors
 *****************************************************************************/

FSessionManager::FSessionManager( const IMessageBusRef& InMessageBus )
	: MessageBusPtr(InMessageBus)
{
	// fill in the owner array
	FString Filter;

	if (FParse::Value(FCommandLine::Get(), TEXT("SessionFilter="), Filter))
	{
		// Allow support for -SessionFilter=Filter1+Filter2+Filter3
		int32 PlusIdx = Filter.Find(TEXT("+"));

		while (PlusIdx != INDEX_NONE)
		{
			FString Owner = Filter.Left(PlusIdx);
			FilteredOwners.Add(Owner);
			Filter = Filter.Right(Filter.Len() - (PlusIdx + 1));
			PlusIdx = Filter.Find(TEXT("+"));
		}

		FilteredOwners.Add(Filter);
	}

	// connect to message bus
	MessageEndpoint = FMessageEndpoint::Builder("FSessionManager", InMessageBus)
		.Handling<FEngineServicePong>(this, &FSessionManager::HandleEnginePongMessage)
		.Handling<FSessionServicePong>(this, &FSessionManager::HandleSessionPongMessage);

	// initialize ticker
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FSessionManager::HandleTicker), 1.f);

	SendPing();
}


FSessionManager::~FSessionManager()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}


/* ISessionManager interface
 *****************************************************************************/

void FSessionManager::AddOwner( const FString& InOwner )
{
	FilteredOwners.Add(InOwner);
}


void FSessionManager::GetSelectedInstances( TArray<ISessionInstanceInfoPtr>& OutInstances) const
{
	if (SelectedSession.IsValid())
	{
		SelectedSession->GetInstances(OutInstances);

		for (int32 Index = OutInstances.Num() - 1; Index >= 0 ; --Index)
		{
			if (DeselectedInstances.Contains(OutInstances[Index]))
			{
				OutInstances.RemoveAtSwap(Index);
			}
		}
	}
}


const ISessionInfoPtr& FSessionManager::GetSelectedSession() const
{
	return SelectedSession;
}


void FSessionManager::GetSessions( TArray<ISessionInfoPtr>& OutSessions ) const
{
	OutSessions.Empty(Sessions.Num());

	for (TMap<FGuid, TSharedPtr<FSessionInfo> >::TConstIterator It(Sessions); It; ++It)
	{
		OutSessions.Add(It.Value());
	}
}


bool FSessionManager::IsInstanceSelected( const TSharedRef<ISessionInstanceInfo>& Instance ) const
{
	return ((Instance->GetOwnerSession() == SelectedSession) && !DeselectedInstances.Contains(Instance));
}


FSimpleMulticastDelegate& FSessionManager::OnSessionsUpdated()
{
	return SessionsUpdatedDelegate;
}


FSimpleMulticastDelegate& FSessionManager::OnSessionInstanceUpdated()
{
	return SessionInstanceUpdatedDelegate;
}


void FSessionManager::RemoveOwner( const FString& InOwner )
{
	FilteredOwners.Remove(InOwner);
	RefreshSessions();
}


bool FSessionManager::SelectSession( const ISessionInfoPtr& Session )
{
	if (Session != SelectedSession)
	{
		if (!Session.IsValid() || Sessions.Contains(Session->GetSessionId()))
		{
			bool CanSelect = true;

			CanSelectSessionDelegate.Broadcast(Session, CanSelect);

			if (CanSelect)
			{
				SelectedSession = Session;

				SelectedSessionChangedEvent.Broadcast(Session);
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}


bool FSessionManager::SetInstanceSelected( const ISessionInstanceInfoPtr& Instance, bool Selected )
{
	if (Instance->GetOwnerSession() == SelectedSession)
	{
		if (Selected)
		{
			if (DeselectedInstances.Remove(Instance) > 0)
			{
				InstanceSelectionChangedDelegate.Broadcast();
			}
		}
		else
		{
			if (!DeselectedInstances.Contains(Instance))
			{
				DeselectedInstances.Add(Instance);
				InstanceSelectionChangedDelegate.Broadcast();
			}
		}

		return true;
	}

	return false;
}


/* FSessionManager implementation
 *****************************************************************************/

void FSessionManager::FindExpiredSessions( const FDateTime& Now )
{
	bool Dirty = false;

	for (TMap<FGuid, TSharedPtr<FSessionInfo> >::TIterator It(Sessions); It; ++It)
	{
		if (Now > It.Value()->GetLastUpdateTime() + FTimespan::FromSeconds(10.0))
		{
			It.RemoveCurrent();
			Dirty = true;
		}
	}

	if (Dirty)
	{
		SessionsUpdatedDelegate.Broadcast();
	}
}


bool FSessionManager::IsValidOwner( const FString& Owner )
{
	if (Owner == FPlatformProcess::UserName(true))
	{
		return true;
	}
	else 
	{
		for (int32 Index = 0; Index < FilteredOwners.Num(); ++Index)
		{
			if (FilteredOwners[Index] == Owner)
			{
				return true;
			}
		}
	}

	return false;
}


void FSessionManager::RefreshSessions()
{
	bool Dirty = false;

	for (TMap<FGuid, TSharedPtr<FSessionInfo> >::TIterator It(Sessions); It; ++It)
	{
		if (!IsValidOwner(It.Value()->GetSessionOwner()))
		{
			It.RemoveCurrent();

			Dirty = true;
		}
	}

	if (Dirty)
	{
		SessionsUpdatedDelegate.Broadcast();
	}
}


void FSessionManager::SendPing()
{
	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Publish(new FEngineServicePing(), EMessageScope::Network);
		MessageEndpoint->Publish(new FSessionServicePing(), EMessageScope::Network);
	}

	LastPingTime = FDateTime::UtcNow();
}


/* FSessionManager callbacks
 *****************************************************************************/

void FSessionManager::HandleEnginePongMessage( const FEngineServicePong& Message, const IMessageContextRef& Context )
{
	if (!Message.SessionId.IsValid())
	{
		return;
	}

	// update instance
	TSharedPtr<FSessionInfo> Session = Sessions.FindRef(Message.SessionId);

	if (Session.IsValid())
	{
		Session->UpdateFromMessage(Message, Context);
		SessionInstanceUpdatedDelegate.Broadcast();
	}
}


void FSessionManager::HandleInstanceDiscovered( const ISessionInfoRef& Session, const ISessionInstanceInfoRef& Instance )
{
	if (Session == SelectedSession)
	{
		InstanceSelectionChangedDelegate.Broadcast();
	}	
}


void FSessionManager::HandleLogReceived( const ISessionInfoRef& Session, const ISessionInstanceInfoRef& Instance, const FSessionLogMessageRef& Message )
{
	if (Session == SelectedSession)
	{
		LogReceivedEvent.Broadcast(Session, Instance, Message);
	}
}


void FSessionManager::HandleSessionPongMessage( const FSessionServicePong& Message, const IMessageContextRef& Context )
{
	if (!Message.SessionId.IsValid())
	{
		return;
	}

	if (Message.Standalone && !IsValidOwner(Message.SessionOwner))
	{
		return;
	}

	IMessageBusPtr MessageBus = MessageBusPtr.Pin();

	if (!MessageBus.IsValid())
	{
		return;
	}

	// update session
	TSharedPtr<FSessionInfo>& Session = Sessions.FindOrAdd(Message.SessionId);

	if (Session.IsValid())
	{
		Session->UpdateFromMessage(Message, Context);
	}
	else
	{
		Session = MakeShareable(new FSessionInfo(Message.SessionId, MessageBus.ToSharedRef()));
		Session->OnInstanceDiscovered().AddSP(this, &FSessionManager::HandleInstanceDiscovered);
		Session->OnLogReceived().AddSP(this, &FSessionManager::HandleLogReceived);
		Session->UpdateFromMessage(Message, Context);

		SessionsUpdatedDelegate.Broadcast();
	}
}


bool FSessionManager::HandleTicker( float DeltaTime )
{
	FDateTime Now = FDateTime::UtcNow();

	// @todo gmp: don't expire sessions for now
//	FindExpiredSessions(Now);

	if (Now >= LastPingTime + FTimespan::FromSeconds(2.5))
	{
		SendPing();
	}

	return true;
}
