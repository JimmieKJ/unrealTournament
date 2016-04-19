// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTPartyInviteWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUTPartyInviteWidget)
	{}

	/** Always at the end */
	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SUTPartyInviteWidget();

	/** Needed for every widget */
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx);

	/** Player context */
	FLocalPlayerContext Ctx;

	EVisibility ShouldShowInviteWidget() const;

	void HandleFriendsActionNotification(TSharedRef<FFriendsAndChatMessage> FriendsAndChatMessage);

	FReply AcceptInvite();
	FReply RejectInvite();

	FString LastInviteContent;
	FString LastInviteUniqueID;
	FDateTime LastInviteTime;

	TSharedPtr<STextBlock> InviteMessage;
};

#endif