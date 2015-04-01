
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"
#include "SUMatchPanel.h"
#include "UTLobbyPC.h"
#include "UTLobbyPlayerState.h"

#if !UE_SERVER

void SUMatchPanel::Construct(const FArguments& InArgs)
{
	bIsFeatured = InArgs._bIsFeatured;
	MatchInfo = InArgs._MatchInfo;
	OnClicked = InArgs._OnClicked;

	PlayerOwner = InArgs._PlayerOwner;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)						// First the overlaid box that controls everything....
				.HeightOverride(192)
				.WidthOverride(192)
				[
					SNew(SButton)
					.OnClicked(this, &SUMatchPanel::ButtonClicked)
					.ButtonStyle(SUWindowsStyle::Get(),"UWindows.Lobby.MatchButton")
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Fill)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(5.0,5.0,5.0,0.0)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(SBox)						// First the overlaid box that controls everything....
								.HeightOverride(145)
								.WidthOverride(145)
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.Padding(5.0,0.0,5.0,0.0)
									.VAlign(VAlign_Fill)
									.HAlign(HAlign_Center)
									[
										SNew(SRichTextBlock)
										.TextStyle(SUWindowsStyle::Get(),"UWindows.Chat.Text.Global")
										.Justification(ETextJustify::Center)
										.DecoratorStyleSet( &SUWindowsStyle::Get() )
										.AutoWrapText( true )
										.Text(this, &SUMatchPanel::GetMatchBadgeText)
									]
								]
							]
						]
						+SVerticalBox::Slot()
						.VAlign(VAlign_Bottom)
						.HAlign(HAlign_Fill)
						.AutoHeight()
						.Padding(20.0f,0.0f,20.0f,5.0f)
						[
							SNew(SBox)
							.HeightOverride(24)
							[
								SNew(SOverlay)
								+SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
								]
								+SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
											.Text( this, &SUMatchPanel::GetButtonText)
											.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.Action.TextStyle")
									]
								]
							]
						]
					]
				]
			]
			
		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(192)
					.HeightOverride(220)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Bottom)
						.AutoWidth()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(32)
								.HeightOverride(32)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										SNew(SImage)
										.Image(this, &SUMatchPanel::GetELOBadgeImage)
									]
									+SOverlay::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Top)
									[
										SNew(SImage)
										.Image(this, &SUMatchPanel::GetELOBadgeNumberImage)
									]
								]
							]
						]
					]
				]

			]
		]
	];
}


const FSlateBrush* SUMatchPanel::GetELOBadgeImage() const
{
	int32 Badge = 0;
	int32 Level = 0;

	if (PlayerOwner.IsValid() && MatchInfo.IsValid())
	{
		AUTLobbyGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GS)
		{
			for (int32 i=0; i<GS->PlayerArray.Num();i++)
			{
				if (GS->PlayerArray[i] && GS->PlayerArray[i]->UniqueId == MatchInfo->OwnerId)
				{
					AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(GS->PlayerArray[i]);
					if (PS)
					{
						UUTLocalPlayer::GetBadgeFromELO(PS->AverageRank, Badge, Level);
						break;
					}
				}
			}
		}
	}
	FString BadgeStr = FString::Printf(TEXT("UT.Badge.%i"), Badge);
	return SUWindowsStyle::Get().GetBrush(*BadgeStr);
}

const FSlateBrush* SUMatchPanel::GetELOBadgeNumberImage() const
{
	int32 Badge = 0;
	int32 Level = 0;

	if (PlayerOwner.IsValid() && MatchInfo.IsValid())
	{
		AUTLobbyGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GS)
		{
			for (int32 i=0; i<GS->PlayerArray.Num();i++)
			{
				if (GS->PlayerArray[i] && GS->PlayerArray[i]->UniqueId == MatchInfo->OwnerId)
				{
					AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(GS->PlayerArray[i]);
					if (PS)
					{
						UUTLocalPlayer::GetBadgeFromELO(PS->AverageRank, Badge, Level);
						break;
					}
				}
			}
		}
	}

	FString BadgeNumberStr = FString::Printf(TEXT("UT.Badge.Numbers.%i"), FMath::Clamp<int32>(Level+1,1,9));
	return SUWindowsStyle::Get().GetBrush(*BadgeNumberStr);
}

FText SUMatchPanel::GetButtonText() const
{
	if (MatchInfo.IsValid())
	{
		if (!MatchInfo->SkillTest(PlayerOwner->GetBaseELORank()))
		{
			return NSLOCTEXT("SUMatchPanel","TooSkilled","SKILL LOCKED");
		}


		return MatchInfo->GetActionText();
	}

	return NSLOCTEXT("SUMatchPanel","Setup","Initializing...");
}



void SUMatchPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UpdateMatchInfo();
}
 
void SUMatchPanel::UpdateMatchInfo()
{
	if (MatchInfo.IsValid())
	{
		SetVisibility(EVisibility::All);
	}
	else
	{
		SetVisibility(EVisibility::Hidden);
	}
}

FReply SUMatchPanel::ButtonClicked()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GWorld, 0));
	if (LocalPlayer)
	{
		AUTLobbyPC* PC = Cast<AUTLobbyPC>(LocalPlayer->PlayerController);
		if (PC)
		{
			AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PC->PlayerState);
			{
				if(PS)
				{
					PS->ServerJoinMatch(MatchInfo.Get());
				}
			}
		}
	}
	return FReply::Handled();
}

FText SUMatchPanel::GetMatchBadgeText() const
{
	if (MatchInfo.IsValid())
	{
		// Build the Args...

		FFormatNamedArguments Args;

		if (MatchInfo->CurrentState != ELobbyMatchState::InProgress)
		{
			for (int32 i = 0; i < MatchInfo->Players.Num(); i++)
			{
				if (MatchInfo->Players[i].IsValid())
				{
					const FString Tag = FString::Printf(TEXT("Player%iName"), i);
					Args.Add(Tag, FText::FromString(MatchInfo->Players[i]->PlayerName));
				}
			}
	
			Args.Add(TEXT("NumPlayers"), FText::AsNumber(MatchInfo->Players.Num()));
		}
		else
		{
			for (int32 i = 0; i < MatchInfo->PlayersInMatchInstance.Num(); i++)
			{
				const FString Tag = FString::Printf(TEXT("Player%iName"), i);
				Args.Add(Tag, FText::FromString(MatchInfo->PlayersInMatchInstance[i].PlayerName));
			}
	
			Args.Add(TEXT("NumPlayers"), FText::AsNumber(MatchInfo->PlayersInMatchInstance.Num()));
		
		}
		return FText::Format(FText::FromString(MatchInfo->MatchBadge), Args);
	}

	return FText::GetEmpty();
}


#endif