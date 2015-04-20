
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
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					// This is the outer box that sizes the whole control
					SNew(SBox)						
					.HeightOverride(192)
					.WidthOverride(165)
					[
						// Here is our button
						SNew(SButton)
						.OnClicked(this, &SUMatchPanel::ButtonClicked)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Top)
							.HAlign(HAlign_Fill)
							.Padding(5.0,5.0,5.0,0.0)
							[

								// Build the Badge Background

								SNew(SOverlay)
								+SOverlay::Slot()
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.HAlign(HAlign_Center)
										.AutoHeight()
										[
											SNew(SBox)
											.HeightOverride(155)
											.WidthOverride(155)
											[
												SNew(SImage)
												.Image(this, &SUMatchPanel::GetMatchSlateBadge)
											]
										]
									]

								]
								+SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Right)
									.FillHeight(1.0)
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot()
										.VAlign(VAlign_Bottom)
										.Padding(0.0,0.0,5.0,3.0)
										.AutoWidth()
										[
											SNew(STextBlock)
											.Text(this, &SUMatchPanel::GetTimeRemaining)
											.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
										]
									]
								]

								+SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Center)
									.FillHeight(1.0)
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot()
										.VAlign(VAlign_Top)
										.Padding(0.0,5.0,0.0,0.0)
										.AutoWidth()
										[
											SNew(STextBlock)
											.Text(this, &SUMatchPanel::GetRuleName)
											.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
										]
									]
								]

								+SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Center)
									.FillHeight(1.0)
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot()
										.VAlign(VAlign_Center)
										.Padding(0.0,5.0,0.0,0.0)
										.AutoWidth()
										[
											SNew(SRichTextBlock)
											.Text(this, &SUMatchPanel::GetMatchBadgeText)
											.TextStyle(SUWindowsStyle::Get(),"UWindows.Chat.Text.Global")
											.Justification(ETextJustify::Center)
											.DecoratorStyleSet( &SUWindowsStyle::Get() )
											.AutoWrapText( true )
										]
									]
								]


								+SOverlay::Slot()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Center)
									.AutoHeight()
									[
										SNew(SHorizontalBox)
										+SHorizontalBox::Slot()
										.AutoWidth()
										[
											SNew(SBox)
											.WidthOverride(155)
											.HeightOverride(155)
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
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text( this, &SUMatchPanel::GetButtonText)
										.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
										.ColorAndOpacity(FLinearColor::Black)
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

FText SUMatchPanel::GetRuleName() const
{
	if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid())
	{
		return FText::FromString(MatchInfo->CurrentRuleset->Title);
	}
	return FText::GetEmpty(); 
}

FText SUMatchPanel::GetTimeRemaining() const
{
	if (MatchInfo.IsValid() && MatchInfo->CurrentState == ELobbyMatchState::InProgress)
	{
		return UUTGameEngine::ConvertTime(FText::GetEmpty(), FText::GetEmpty(), MatchInfo->MatchGameTime, true, true, true); 
	}

	return FText::GetEmpty();
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
	if (MatchInfo.IsValid() && MatchInfo->CurrentState == ELobbyMatchState::InProgress)
	{
		return FText::FromString(MatchInfo->MatchBadge);
	}
	else if ( MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid() )
	{
		FString Text = FString::Printf(TEXT("<UWindows.Standard.MatchBadge>%i of %i</>\n<UWindows.Standard.MatchBadge>Players</>"), MatchInfo->Players.Num(), MatchInfo->CurrentRuleset->MaxPlayers);
		return FText::FromString(Text);
	}
	else
	{
		return FText::GetEmpty();
	}

	return FText::GetEmpty();
}

const FSlateBrush* SUMatchPanel::GetMatchSlateBadge() const
{
	if (MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid() && MatchInfo->CurrentRuleset->SlateBadge.IsValid())
	{
		return MatchInfo->CurrentRuleset->GetSlateBadge();
	
	}

	return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");

}

#endif