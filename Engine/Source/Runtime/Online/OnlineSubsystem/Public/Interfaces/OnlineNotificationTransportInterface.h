// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineDelegateMacros.h"
#include "OnlineSubsystemTypes.h"

struct FOnlineNotification;

// abstract base class for messages of the type understood by the specific transport mechanism, eg. xmpp
class IOnlineNotificationTransportMessage
{

};


/**
* Interface for notification transport mechanisms
*/
class IOnlineNotificationTransport
{
protected:
	/** Constructor */
	IOnlineNotificationTransport(IOnlineSubsystem* InOnlineSubsystemInstance, FNotificationTransportId InTransportId)
		: OnlineSubsystemInstance(InOnlineSubsystemInstance),Id(InTransportId)
	{
	}

	virtual ~IOnlineNotificationTransport()
	{
	}

	/** The OSS associated with this transport, used for accessing the notification handler and transport manager */
	IOnlineSubsystem* OnlineSubsystemInstance;

	/** Unique notification transport id associated with this transport */
	FNotificationTransportId Id;

public:

	const FNotificationTransportId& GetNotificationTransportId() const
	{
		return Id;
	}

	/**
	* Equality operator
	*/
	bool operator==(const IOnlineNotificationTransport& Other) const
	{
		return Other.GetNotificationTransportId() == Id;
	}

	virtual const IOnlineNotificationTransportMessage* Convert(const FOnlineNotification& Notification) = 0;

	virtual const FOnlineNotification& Convert(const IOnlineNotificationTransportMessage* TransportMessage) = 0;

	/** Send a notification out using this transport mechanism */
	virtual bool SendNotification(const FOnlineNotification& Notification) = 0;

	/** Receive a transport-specific notification in from this transport mechanism and pass along to be delivered */
	virtual bool ReceiveNotification(const IOnlineNotificationTransportMessage& TransportMessage) = 0;
};

typedef TSharedPtr<IOnlineNotificationTransport, ESPMode::ThreadSafe> IOnlineNotificationTransportPtr;