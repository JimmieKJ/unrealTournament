// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

enum class EPartyType : uint8;

class UNREALTOURNAMENT_API SUTPartyWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUTPartyWidget)
	{}

	/** Always at the end */
	SLATE_END_ARGS() 

	/** Needed for every widget */
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx);

private:

	FReply PlayerNameClicked(int32 PartyMemberIdx);
	FReply KickFromParty(int32 PartyMemberIdx);
	FReply PromoteToLeader(int32 PartyMemberIdx);
	FReply LeaveParty();
	FReply InviteToParty(FString UserId);

	FReply ChangePartyType(EPartyType InPartyType);

	FReply AllowMemberFriends(bool bAllow);
	FReply AllowMemberInvites(bool bAllow);

	void LeaderButtonClicked();

	void PartyStateChanged();
	void PartyLeft();
	void PartyMemberPromoted();

	void SetupPartyMemberBox();
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TSharedPtr<SHorizontalBox> PartyMemberBox;

	/** Player context */
	FLocalPlayerContext Ctx;
	
	bool bPartyMenuCollapsed;

	float RefreshTimer;
	int32 LastFriendCount;
};

#endif