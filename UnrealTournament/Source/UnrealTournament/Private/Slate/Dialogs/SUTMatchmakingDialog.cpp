// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTMatchmakingDialog.h"
#include "UTGameInstance.h"
#include "UTParty.h"
#include "UTMatchmaking.h"
#include "UTPartyGameState.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTMatchmakingDialog::Construct(const FArguments& InArgs)
{
	LastMatchmakingPlayersNeeded = 0;
	RetryTime = 120.0f;
	RetryCountdown = RetryTime;
	
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(InArgs._DialogTitle)
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(InArgs._ButtonMask)
		.OnDialogResult(InArgs._OnDialogResult)
		);

	TimeDialogOpened = PlayerOwner->GetWorld()->RealTimeSeconds;

	if (DialogContent.IsValid())
	{		
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SUTMatchmakingDialog::GetRegionText)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SUTMatchmakingDialog::GetMatchmakingText)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SUTMatchmakingDialog::GetMatchmakingText2)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 15.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SUTMatchmakingDialog::GetMatchmakingTimeElapsedText)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SUTMatchmakingDialog::GetMatchmakingEstimatedTimeText)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		];
	}
}

FText SUTMatchmakingDialog::GetRegionText() const
{
	UUTGameInstance* GameInstance = Cast<UUTGameInstance>(GetPlayerOwner()->GetGameInstance());
	if (GameInstance)
	{
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UUTPartyGameState* PartyState = Party->GetUTPersistentParty();
			if (PartyState)
			{
				FString MatchMakingRegion = PartyState->GetMatchmakingRegion();
				if (PartyState->GetPartyProgression() == EUTPartyState::QuickMatching)
				{
					return FText::Format(NSLOCTEXT("Generic", "QuickMatching", "Quick Matching in Region: {0}"), FText::FromString(MatchMakingRegion));
				}
				else
				{
					return FText::Format(NSLOCTEXT("Generic", "Region", "Region: {0}"), FText::FromString(MatchMakingRegion));
				}
			}
		}
	}

	return NSLOCTEXT("Generic", "LeaderMatchmaking", "Leader Is Matchmaking");
}

FText SUTMatchmakingDialog::GetMatchmakingText() const
{
	UUTGameInstance* GameInstance = Cast<UUTGameInstance>(GetPlayerOwner()->GetGameInstance());
	if (GameInstance)
	{
		UUTMatchmaking* Matchmaking = GameInstance->GetMatchmaking();
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UUTPartyGameState* PartyState = Party->GetUTPersistentParty();
			if (PartyState)
			{
				switch (PartyState->GetPartyProgression())
				{
				case EUTPartyState::PostMatchmaking:
					if (Matchmaking && Matchmaking->IsRankedMatchmaking())
					{
						return FText::Format(NSLOCTEXT("Generic", "WaitingOnOtherPlayers", "Server Found. Waiting For {0} Players To Join..."), FText::AsNumber(PartyState->GetMatchmakingPlayersNeeded()));
					}
					else
					{
						return NSLOCTEXT("Generic", "QMJoiningServer", "Server Found. Joining shortly...");
					}
				}
			}
		}

		if (Matchmaking)
		{
			int32 MatchmakingTeamElo = Matchmaking->GetMatchmakingTeamElo();
			if (MatchmakingTeamElo > 0)
			{
				return FText::Format(NSLOCTEXT("Generic", "SearchingTeamElo", "Your Team ELO is {0}."), FText::AsNumber(MatchmakingTeamElo));
			}
		}
	}

	return NSLOCTEXT("Generic", "SearchingForServer", "Searching For Server...");
}

FText SUTMatchmakingDialog::GetMatchmakingText2() const
{
	UUTGameInstance* GameInstance = Cast<UUTGameInstance>(GetPlayerOwner()->GetGameInstance());
	if (GameInstance)
	{
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UUTPartyGameState* PartyState = Party->GetUTPersistentParty();
			if (PartyState && PartyState->GetPartyProgression() == EUTPartyState::PostMatchmaking)
			{
				if (PlayerOwner->IsPartyLeader())
				{
					FNumberFormattingOptions NumberFormattingOptions;
					NumberFormattingOptions.MaximumFractionalDigits = 0;
					return FText::Format(NSLOCTEXT("Generic", "ResearchingTime", "Will Restart Search In {0} Seconds"), FText::AsNumber(RetryCountdown, &NumberFormattingOptions));
				}
				else
				{
					return FText::GetEmpty();
				}
			}
		}

		UUTMatchmaking* Matchmaking = GameInstance->GetMatchmaking();
		if (Matchmaking)
		{
			int32 MatchmakingTeamElo = Matchmaking->GetMatchmakingTeamElo();
			int32 MatchmakingEloRange = Matchmaking->GetMatchmakingEloRange();
			if (MatchmakingEloRange > 0 && MatchmakingTeamElo > 0)
			{
				return FText::Format(NSLOCTEXT("Generic", "SearchingForServerWithEloRange", "Searching For Server Within ELO Between {0} and {1}..."), FText::AsNumber(MatchmakingTeamElo - MatchmakingEloRange), FText::AsNumber(MatchmakingTeamElo + MatchmakingEloRange));
			}
		}
	}

	return FText::GetEmpty();
}

FText SUTMatchmakingDialog::GetMatchmakingTimeElapsedText() const
{
	UUTGameInstance* GameInstance = Cast<UUTGameInstance>(GetPlayerOwner()->GetGameInstance());
	if (GameInstance)
	{
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UUTPartyGameState* PartyState = Party->GetUTPersistentParty();
			if (PartyState)
			{
				if (PlayerOwner->IsPartyLeader())
				{
					int32 ElapsedTime = PlayerOwner->GetWorld()->RealTimeSeconds - TimeDialogOpened;
					FTimespan TimeSpan(0, 0, ElapsedTime);
					return FText::Format(NSLOCTEXT("Generic", "ElapsedMatchMakingTime", "You Have Been Matchmaking For {0}"), FText::AsTimespan(TimeSpan));
				}
			}
		}
	}
	
	return FText::GetEmpty();	
}

FText SUTMatchmakingDialog::GetMatchmakingEstimatedTimeText() const
{
	UUTGameInstance* GameInstance = Cast<UUTGameInstance>(GetPlayerOwner()->GetGameInstance());
	if (GameInstance)
	{
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UUTPartyGameState* PartyState = Party->GetUTPersistentParty();
			if (PartyState)
			{
				if (PlayerOwner->IsPartyLeader())
				{
					UUTMatchmaking* Matchmaking = GameInstance->GetMatchmaking();
					if (Matchmaking)
					{
						int32 EstimatedWaitTime = Matchmaking->GetEstimatedMatchmakingTime();
						if (EstimatedWaitTime > 0 && EstimatedWaitTime < 60*10)
						{
							FTimespan TimeSpan(0, 0, EstimatedWaitTime);
							return FText::Format(NSLOCTEXT("Generic", "EstimateMatchMakingTime", "Estimated Wait Time {0}"), FText::AsTimespan(TimeSpan));
						}
					}
				}
			}
		}
	}

	return FText::GetEmpty();
}

FReply SUTMatchmakingDialog::OnButtonClick(uint16 ButtonID)
{
	OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

void SUTMatchmakingDialog::Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SUTDialogBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Failsafe in case we join a server
	if (PlayerOwner.IsValid() && PlayerOwner->GetWorld()->GetNetMode() == NM_Client)
	{
		GetPlayerOwner()->CloseDialog(SharedThis(this));
	}

	if (PlayerOwner.IsValid() && PlayerOwner->IsMenuGame() && PlayerOwner->IsPartyLeader())
	{
		UUTGameInstance* GameInstance = Cast<UUTGameInstance>(GetPlayerOwner()->GetGameInstance());
		if (GameInstance)
		{
			UUTParty* Party = GameInstance->GetParties();
			if (Party)
			{
				UUTPartyGameState* PartyState = Party->GetUTPersistentParty();
				if (PartyState && PartyState->GetPartyProgression() == EUTPartyState::PostMatchmaking)
				{
					const int32 MatchmakingPlayersNeeded = PartyState->GetMatchmakingPlayersNeeded();
					if (MatchmakingPlayersNeeded > 0)
					{
						if (MatchmakingPlayersNeeded == LastMatchmakingPlayersNeeded)
						{
							RetryCountdown -= InDeltaTime;
							if (RetryCountdown < 0)
							{
								UUTMatchmaking* Matchmaking = GameInstance->GetMatchmaking();
								if (Matchmaking)
								{
									// Disconnect from current server
									// Start matchmaking with a higher elo gate for starting own server and skip joining current server
									Matchmaking->RetryFindGatheringSession();
									LastMatchmakingPlayersNeeded = 0;
									RetryCountdown = RetryTime;
								}
							}
						}
						else
						{
							LastMatchmakingPlayersNeeded = MatchmakingPlayersNeeded;
							RetryCountdown = RetryTime;
						}
					}
				}
			}
		}
	}
}


#endif