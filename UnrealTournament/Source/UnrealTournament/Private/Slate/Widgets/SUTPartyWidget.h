// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTPartyWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUTPartyWidget)
	{}

	/** Always at the end */
	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SUTPartyWidget();

	/** Needed for every widget */
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx);

private:

	FReply PlayerNameClicked(int32 PartyMemberIdx);
	FReply KickFromParty(int32 PartyMemberIdx);
	FReply PromoteToLeader(int32 PartyMemberIdx);

	void PartyStateChanged();

	void SetupPartyMemberBox();

	TSharedPtr<SHorizontalBox> PartyMemberBox;

	/** Player context */
	FLocalPlayerContext Ctx;
};

#endif