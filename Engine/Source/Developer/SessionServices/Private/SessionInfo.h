// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a class to maintain all info related to a game session
 */
class FSessionInfo
	: public TSharedFromThis<FSessionInfo>
	, public ISessionInfo
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InSessionId The session's identifier.
	 * @param InMessageBus The message bus to use.
	 */
	FSessionInfo( const FGuid& InSessionId, const IMessageBusRef& InMessageBus )
		: MessageBusPtr(InMessageBus)
		, SessionId(InSessionId)
	{ }

public:

	/**
	 * Updates this session info with the data in the specified message.
	 *
	 * @param Message The message containing engine information.
	 * @param Context The message context.
	 */
	void UpdateFromMessage( const FEngineServicePong& Message, const IMessageContextRef& Context )
	{
		if (Message.SessionId != SessionId)
		{
			return;
		}

		// update instance
		// @todo gmp: reconsider merging EngineService and SessionService
		/*TSharedPtr<FSessionInstanceInfo> Instance = Instances.FindRef(Context->GetSender());

		if (Instance.IsValid())
		{
			Instance->UpdateFromMessage(Message, Context);
		}*/
		for (TMap<FMessageAddress, TSharedPtr<FSessionInstanceInfo> >::TIterator It(Instances); It; ++It)
		{
			if (It.Value()->GetInstanceId() == Message.InstanceId)
			{
				It.Value()->UpdateFromMessage(Message, Context);
				break;
			}
		}
	}

	/**
	 * Updates this session info with the data in the specified message.
	 *
	 * @param Message The message containing session information.
	 * @param Context The message context.
	 */
	void UpdateFromMessage( const FSessionServicePong& Message, const IMessageContextRef& Context )
	{
		if (Message.SessionId != SessionId)
		{
			return;
		}

		// update session info
		Standalone = Message.Standalone;
		SessionOwner = Message.SessionOwner;

		if (SessionName.IsEmpty())
		{
			SessionName = Message.SessionName;
		}

		// update instance
		TSharedPtr<FSessionInstanceInfo>& Instance = Instances.FindOrAdd(Context->GetSender());

		if (Instance.IsValid())
		{
			Instance->UpdateFromMessage(Message, Context);
		}
		else
		{
			IMessageBusPtr MessageBus = MessageBusPtr.Pin();

			if (MessageBus.IsValid())
			{
				Instance = MakeShareable(new FSessionInstanceInfo(Message.InstanceId, AsShared(), MessageBus.ToSharedRef()));
				Instance->OnLogReceived().AddSP(this, &FSessionInfo::HandleLogReceived);
				Instance->UpdateFromMessage(Message, Context);

				InstanceDiscoveredDelegate.Broadcast(AsShared(), Instance.ToSharedRef());
			}
		}

		LastUpdateTime = FDateTime::UtcNow();
	}

public:	

	// SessionInfo interface

	virtual void GetInstances( TArray<ISessionInstanceInfoPtr>& OutInstances ) const override
	{
		OutInstances.Empty();

		for (TMap<FMessageAddress, TSharedPtr<FSessionInstanceInfo> >::TConstIterator It(Instances); It; ++It)
		{
			OutInstances.Add(It.Value());
		}
	}

	virtual FDateTime GetLastUpdateTime() override
	{
		return LastUpdateTime;
	}

	virtual const int32 GetNumInstances() const override
	{
		return Instances.Num();
	}

	virtual const FGuid& GetSessionId() const override
	{
		return SessionId;
	}

	virtual const FString& GetSessionName() const override
	{
		return SessionName;
	}

	virtual const FString& GetSessionOwner() const override
	{
		return SessionOwner;
	}

	virtual const bool IsStandalone() const override
	{
		return Standalone;
	}

	virtual FOnSessionInstanceDiscovered& OnInstanceDiscovered() override
	{
		return InstanceDiscoveredDelegate;
	}

	virtual FOnSessionLogReceived& OnLogReceived() override
	{
		return LogReceivedDelegate;
	}

	virtual void Terminate() override
	{
		for (TMap<FMessageAddress, TSharedPtr<FSessionInstanceInfo> >::TIterator It(Instances); It; ++It)
		{
			It.Value()->Terminate();
		}
	}

private:

	/** Handles received log messages. */
	void HandleLogReceived( const ISessionInstanceInfoRef& Instance, const FSessionLogMessageRef& LogMessage )
	{
		LogReceivedDelegate.Broadcast(AsShared(), Instance, LogMessage);
	}

private:

	/** Holds the list of engine instances that belong to this session. */
	TMap<FMessageAddress, TSharedPtr<FSessionInstanceInfo>> Instances;

	/** Holds the time at which the last pong was received. */
	FDateTime LastUpdateTime;

	/** Holds a weak pointer to the message bus. */
	IMessageBusWeakPtr MessageBusPtr;

	/** Holds the session identifier. */
	FGuid SessionId;

	/** Holds the session name. */
	FString SessionName;

	/** Holds the name of the user who launched the session. */
	FString SessionOwner;

	/** Whether the session is local. */
	bool Standalone;

private:

	/** Holds a delegate to be invoked when a new instance has been discovered. */
	FOnSessionInstanceDiscovered InstanceDiscoveredDelegate;

	/** Holds a delegate to be invoked when an instance received a log message. */
	FOnSessionLogReceived LogReceivedDelegate;
};
