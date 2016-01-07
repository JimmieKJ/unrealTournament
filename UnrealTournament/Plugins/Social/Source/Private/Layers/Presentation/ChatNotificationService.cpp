// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatNotificationService.h"
#include "ClanInfo.h"

#define LOCTEXT_NAMESPACE ""

class FChatNotificationServiceImpl
	: public FChatNotificationService
{
public:

private:

	void Initialize()
	{
	}

	FChatNotificationServiceImpl()
	{
	}

	virtual void SendNotification(const FString& InMessage, EMessageType::Type MessageType) override
	{
		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(InMessage));
		NotificationMessage->SetMessageType(MessageType);
		OnSendNotification().Broadcast(NotificationMessage.ToSharedRef());
	}

	virtual void SendFriendInviteSentNotification(const FString& FriendDisplayName) override
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Username"), FText::FromString(FriendDisplayName));
		const FText SentInviteMessage = FText::Format(LOCTEXT("SentFriendRequestToast", "Friend request sent to {Username}"), Args);
		SendNotification(SentInviteMessage.ToString(), EMessageType::FriendInviteSent);
	}

	virtual void SendFriendInviteNotification(const TSharedPtr<IFriendItem>& Invite, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) override
	{
		FString MessageString = FString::Printf(*LOCTEXT("AddedYou", "Friend request from %s").ToString(), *Invite->GetName());
		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(MessageString, Invite->GetUniqueID()));
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Accept, ResponceDelegate));
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Ignore, ResponceDelegate));
		NotificationMessage->SetButtonDescription(LOCTEXT("Accept", "Accept"));
		NotificationMessage->SetButtonDescription(LOCTEXT("Ignore", "Ignore"));
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Accept);
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Ignore);
		NotificationMessage->SetButtonStyle(TEXT("FriendsListEmphasisButton"));
		NotificationMessage->SetButtonStyle(TEXT("FriendsListCriticalButton"));
		NotificationMessage->SetMessageType(EMessageType::FriendInvite);
		OnSendNotification().Broadcast(NotificationMessage.ToSharedRef());
	}

	virtual void SendAcceptInviteNotification(const TSharedPtr< IFriendItem >& Friend) override
	{
		if (Friend->IsPendingAccepted())
		{
			SendNotification(FString::Printf(*LOCTEXT("FriendAddedToast", "%s added as a friend").ToString(), *Friend->GetName()), EMessageType::FriendAccepted);
		}
		else
		{
			SendNotification(FString::Printf(*LOCTEXT("FriendAcceptedToast", "%s accepted your request").ToString(), *Friend->GetName()), EMessageType::FriendAccepted);
		}
	}

	virtual void SendMessageReceivedNotification(const TSharedPtr< IFriendItem >& Friend) override
	{
		if (Friend.IsValid())
		{
			SendNotification(FString::Printf(*LOCTEXT("FriendMessageRecievedToast", "Message from %s").ToString(), *Friend->GetName()), EMessageType::ChatMessage);
		}
	}

	virtual void SendGameInviteNotification(const TSharedPtr<IFriendItem>& Invite, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) override
	{
		FString MessageString = FString::Printf(*LOCTEXT("GameInvite", "Party invite from %s").ToString(), *Invite->GetName());
		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(MessageString, Invite->GetUniqueID()));
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Accept, ResponceDelegate));
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Reject, ResponceDelegate));
		NotificationMessage->SetButtonDescription(LOCTEXT("JoinGame", "Join Game"));
		NotificationMessage->SetButtonDescription(LOCTEXT("Reject", "Reject"));
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Accept);
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Reject);
		NotificationMessage->SetButtonStyle(TEXT("FriendsListEmphasisButton"));
		NotificationMessage->SetButtonStyle(TEXT("FriendsListCriticalButton"));
		NotificationMessage->SetMessageType(EMessageType::GameInvite);
		OnSendNotification().Broadcast(NotificationMessage.ToSharedRef());
	}

	virtual void SendNewClanMemberNotification(const TSharedPtr< IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend) override
	{
		SendNotification(FString::Printf(*LOCTEXT("NewClanMember", "%s joined %s").ToString(), *Friend->GetName(), *Clan->GetTitle().ToString()), EMessageType::FriendAccepted);
	}

	virtual void SendClanMemberLeftNotification(const TSharedPtr< IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend) override
	{
		SendNotification(FString::Printf(*LOCTEXT("ClanMemberLeft", "%s left %s").ToString(), *Friend->GetName(), *Clan->GetTitle().ToString()), EMessageType::FriendAccepted);
	}

	virtual void SendClanInviteReceived(const TSharedPtr< IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) override
	{
		FString MessageString = FString::Printf(*LOCTEXT("ClanInvite", "Clan invite from %s").ToString(), *Clan->GetTitle().ToString());
		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(MessageString, Friend->GetUniqueID()));
		NotificationMessage->SetClanID(Clan->GetUniqueID());
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Accept, ResponceDelegate));
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Reject, ResponceDelegate));
		NotificationMessage->SetButtonDescription(LOCTEXT("JoinClan", "Join Clan"));
		NotificationMessage->SetButtonDescription(LOCTEXT("Reject", "Reject"));
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Accept);
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Reject);
		NotificationMessage->SetButtonStyle(TEXT("FriendsListEmphasisButton"));
		NotificationMessage->SetButtonStyle(TEXT("FriendsListCriticalButton"));
		NotificationMessage->SetMessageType(EMessageType::ClanInvite);
		OnSendNotification().Broadcast(NotificationMessage.ToSharedRef());
	}

	virtual void SendClanJoinRequest(const TSharedPtr< IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) override
	{
		FString MessageString = FString::Printf(*LOCTEXT("ClanJoinRequest", "Clan join request from %s").ToString(), *Friend->GetName());
		TSharedPtr< FFriendsAndChatMessage > NotificationMessage = MakeShareable(new FFriendsAndChatMessage(MessageString, Friend->GetUniqueID()));
		NotificationMessage->SetClanID(Clan->GetUniqueID());
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Accept, ResponceDelegate));
		NotificationMessage->SetButtonCallback(FOnClicked::CreateSP(this, &FChatNotificationServiceImpl::HandleMessageResponse, NotificationMessage, EResponseType::Response_Reject, ResponceDelegate));
		NotificationMessage->SetButtonDescription(LOCTEXT("Accept", "Accept"));
		NotificationMessage->SetButtonDescription(LOCTEXT("Reject", "Reject"));
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Accept);
		NotificationMessage->SetButtonResponseType(EResponseType::Response_Reject);
		NotificationMessage->SetButtonStyle(TEXT("FriendsListEmphasisButton"));
		NotificationMessage->SetButtonStyle(TEXT("FriendsListCriticalButton"));
		NotificationMessage->SetMessageType(EMessageType::ClanInvite);
		OnSendNotification().Broadcast(NotificationMessage.ToSharedRef());
	}

	FReply HandleMessageResponse(TSharedPtr< FFriendsAndChatMessage > ChatMessage, EResponseType::Type ResponseType, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate)
	{
		ResponceDelegate.ExecuteIfBound(ChatMessage.ToSharedRef(), ResponseType);
		return FReply::Handled();
	}

private:

	DECLARE_DERIVED_EVENT(FChatNotificationServiceImpl, IChatNotificationService::FOnNotificationAvailableEvent, FOnNotificationAvailableEvent)
	virtual FOnNotificationAvailableEvent& OnNotificationsAvailable() override
	{
		return NotificationAvailableDelegate;
	}

	DECLARE_DERIVED_EVENT(FChatNotificationServiceImpl, IChatNotificationService::FOnSendNotificationEvent, FOnSendNotificationEvent)
	virtual FOnSendNotificationEvent& OnSendNotification() override
	{
		return SendNotificationDelegate;
	}

	FOnNotificationAvailableEvent NotificationAvailableDelegate;
	FOnSendNotificationEvent SendNotificationDelegate;

	friend FChatNotificationServiceFactory;
};

TSharedRef< FChatNotificationService > FChatNotificationServiceFactory::Create()
{
	TSharedRef< FChatNotificationServiceImpl > NotificationService = MakeShareable(new FChatNotificationServiceImpl);
	NotificationService->Initialize();
	return NotificationService;
}

#undef LOCTEXT_NAMESPACE
