// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IChatNotificationService
{
public:

	virtual void SendNotification(const FString& InMessage, EMessageType::Type) = 0;

	virtual void SendMessageReceivedNotification(const TSharedPtr< IFriendItem >& Friend) = 0;

	DECLARE_EVENT_OneParam(IChatNotificationService, FOnNotificationAvailableEvent, bool)
	virtual FOnNotificationAvailableEvent& OnNotificationsAvailable() = 0;

	DECLARE_EVENT_OneParam(IChatNotificationService, FOnSendNotificationEvent, TSharedRef<FFriendsAndChatMessage> /*Chat notification*/)
	virtual FOnSendNotificationEvent& OnSendNotification() = 0;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNotificationResponse, TSharedPtr<FFriendsAndChatMessage>, EResponseType::Type);
	typedef FOnNotificationResponse::FDelegate FOnNotificationResponseDelegate;
};