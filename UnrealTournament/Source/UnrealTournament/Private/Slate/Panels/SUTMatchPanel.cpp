
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMatchPanel.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../Widgets/SUTPopOverAnchor.h"
#include "SUTServerBrowserPanel.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"
#include "UTLobbyPC.h"
#include "UTLobbyPlayerState.h"
#if !UE_SERVER

struct FMatchComparePlayersByScore {FORCEINLINE bool operator()( const FMatchPlayerListStruct A, const FMatchPlayerListStruct B ) const { return ( A.PlayerScore > B.PlayerScore);}};

void SUTMatchPanel::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	bShowingNoMatches = false;
	OnJoinMatchDelegate = InArgs._OnJoinMatchDelegate;
	bSuspendPopups = false;
	bExpectLiveData = InArgs._bExpectLiveData;

	TSharedPtr<SVerticalBox> VertBox;

	ChildSlot.VAlign(VAlign_Fill).HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(VertBox, SVerticalBox)
		]
	];

	if (ShouldUseLiveData())
	{
		VertBox->AddSlot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		[
			SNew(SBox)												 
			.HeightOverride(75)
			.WidthOverride(954)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
				]
				+SOverlay::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(22.0,0.0,0.0,0.0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SUTMatchPanel::GetServerNameText)
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
					]
				]

				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10.0,5.0,21.0,5.0)

						.HAlign(HAlign_Right)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoHeight()
							[
								SAssignNew(DownloadContentButton, SUTButton)
								.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
								.OnClicked(this, &SUTMatchPanel::DownloadAllButtonClicked)
								.ContentPadding(FMargin(32.0,5.0,32.0,5.0))
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUTMatchPanel","DownloadContent","Download All Content"))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]
							]
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(10.0,5.0,21.0,5.0)

						.HAlign(HAlign_Right)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoHeight()
							[
								SNew(SButton)
								.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton")
								.OnClicked(this, &SUTMatchPanel::StartNewMatch)
								.ContentPadding(FMargin(32.0,5.0,32.0,5.0))
								[
									SNew(STextBlock)
									.Text(this, &SUTMatchPanel::GetMatchButtonText)
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]
							]
						]
					]
				]
			]
		];

		AUTLobbyGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>();
		DownloadContentButton->SetVisibility(GameState && GameState->bCustomContentAvailable ? EVisibility::Visible : EVisibility::Hidden);
	}

	VertBox->AddSlot()
	.VAlign(VAlign_Top)
	.AutoHeight()
	.Padding(0,12,0,0)
	[
		SNew(SBox)												
		.WidthOverride(954).HeightOverride(893)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Medium"))
			]

			+SOverlay::Slot()
			[
				SAssignNew( NoMatchesBox, SVerticalBox)
			]

			+SOverlay::Slot()
			[
				SAssignNew( MatchList, SListView< TSharedPtr<FTrackedMatch> > )
				// List view items are this tall
				.ItemHeight(80)
				// Tell the list view where to get its source data
				.ListItemsSource( &TrackedMatches)
				// When the list view needs to generate a widget for some data item, use this method
				.OnGenerateRow( this, &SUTMatchPanel::OnGenerateWidgetForMatchList )
				.SelectionMode(ESelectionMode::Single)
				.OnMouseButtonDoubleClick(this, &SUTMatchPanel::OnListMouseButtonDoubleClick)
			]
		]
	];
}

bool SUTMatchPanel::ShouldUseLiveData()
{
	return bExpectLiveData; 
}

TSharedRef<ITableRow> SUTMatchPanel::OnGenerateWidgetForMatchList( TSharedPtr<FTrackedMatch> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	int32 Index = TrackedMatches.Find(InItem) + 1;
	
	if (InItem->MatchInfo.IsValid())
	{
		InItem->MatchInfo->TrackedMatchId = Index;
	}

	return SNew(STableRow<TSharedPtr<FSimpleListData>>, OwnerTable)
		.Style(SUTStyle::Get(),"UT.List.Row")
		.Padding(FMargin(1.0,1.0,1.0,1.0))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(SUTPopOverAnchor)
				.OnGetPopoverWidget(this, &SUTMatchPanel::OnGetPopup, InItem)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(78).HeightOverride(78)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SImage)
								.Image(SUTStyle::Get().GetBrush("UT.MatchBadge.Circle.Thin"))
							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.FillHeight(1.0)
								.VAlign(VAlign_Center)
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot()
									.FillWidth(1.0)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::AsNumber(Index))
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									]
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBox).HeightOverride(78).WidthOverride(408).Padding(FMargin(5.0f, 0.0f, 5.0f, 0.0f))
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::GetRuleTitle)))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
									.ColorAndOpacity(FSlateColor(FLinearColor(0.8f,0.8f,0.8f,1.0f)))
								]
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::GetMap)))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									.ColorAndOpacity(FSlateColor(FLinearColor(0.6f,0.6f,0.6f,1.0f)))
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(155).HeightOverride(78)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.FillHeight(1.0)
							.VAlign(VAlign_Bottom)
							[
								SNew(SRichTextBlock)
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny")
								.Justification(ETextJustify::Left)
								.DecoratorStyleSet( &SUTStyle::Get() )
								.AutoWrapText( true )
								.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::GetFlags, PlayerOwner)))
								+ SRichTextBlock::ImageDecorator()
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(103).HeightOverride(78)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.FillHeight(1.0)
							.VAlign(VAlign_Center)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.HAlign(HAlign_Right)
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::GetCurrentPlayerCount)))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
									.ColorAndOpacity(FSlateColor(FLinearColor(0.8f,0.8f,0.8f,1.0f)))
								]
								+SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Right)
								[
									SNew(STextBlock)
									.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::GetMaxPlayers)))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									.ColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.5f,0.5f,1.0f)))
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(92).HeightOverride(78)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoHeight()
								.Padding(16.0,0.0,0.0,0.0)
								[
									SNew(SBox).WidthOverride(64).HeightOverride(64)
									[

										SNew(SOverlay)
										+SOverlay::Slot()
										[
											SNew(SImage)
											.Image(InItem.Get(), &FTrackedMatch::GetBadge)
										]
										+SOverlay::Slot()
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.FillHeight(1.0)
											.VAlign(VAlign_Center)
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												.FillWidth(1.0)
												.HAlign(HAlign_Center)
												[
													SNew(STextBlock)
													.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::GetRank)))
													.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
													.ShadowOffset(FVector2D(0.0f,2.0f))
													.ShadowColorAndOpacity(FLinearColor(0.0f,0.0f,0.0f,1.0f))
												]
											]
										]
									]
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(0.0,0.0,5.0,0.0)
					[
						SNew(SBox).WidthOverride(115).HeightOverride(78)
						[
							SNew(SUTButton)
							.ButtonStyle(SUTStyle::Get(),"UT.ClearButton")
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.Padding(0.0,4.0,0.0,0.0)
								.AutoHeight()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Medium")
									.OnClicked(this, &SUTMatchPanel::JoinMatchButtonClicked, InItem)
									.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::CanJoin)))
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight()
										[
											SNew(STextBlock)
											.Text(NSLOCTEXT("SUTMatchPanel","JoinText","JOIN"))
											.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
										]
									]
								]
								+SVerticalBox::Slot()
								.Padding(0.0,10.0,0.0,0.0)
								.AutoHeight()
								[
									SNew(SButton)
									.ButtonStyle(SUTStyle::Get(),"UT.SimpleButton.Medium")
									.OnClicked(this, &SUTMatchPanel::SpectateMatchButtonClicked, InItem)
									.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(InItem.Get(), &FTrackedMatch::CanSpectate)))
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight()
										[
											SNew(STextBlock)
											.Text(NSLOCTEXT("SUTMatchPanel","SpectateText","SPECTATE"))
											.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
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


FText SUTMatchPanel::GetServerNameText() const
{
	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		return FText::FromString(LobbyGameState->ServerName);
	}

	return FText::GetEmpty();
}

FText SUTMatchPanel::GetMatchButtonText() const
{
	return NSLOCTEXT("Gerneric","NewMatch","START NEW MATCH");
}

int32 SUTMatchPanel::IsTrackingMatch(AUTLobbyMatchInfo* Match)
{
	for (int32 i = 0; i < TrackedMatches.Num(); i++)
	{
		if (TrackedMatches[i]->MatchId == Match->UniqueMatchID)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void SUTMatchPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// If this 
	if (ShouldUseLiveData())
	{
		// Flag all matches as pending kill.  
		for (int32 i = 0; i < TrackedMatches.Num(); i++)
		{
			TrackedMatches[i]->bPendingKill = true;
		}

		bool bListNeedsUpdate = false;

		AUTLobbyGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GameState)
		{
			// Look at the available matches
			for (int32 i = 0; i < GameState->AvailableMatches.Num(); i++)
			{
				AUTLobbyMatchInfo* Match = GameState->AvailableMatches[i];
				if (Match && GameState->IsMatchStillValid(Match) && Match->ShouldShowInDock())
				{
					int32 Idx = IsTrackingMatch(Match);
					if (Idx != INDEX_NONE)
					{
						TrackedMatches[Idx]->bPendingKill = false;
					}
					else
					{
						// We need to add this match
						TrackedMatches.Add(FTrackedMatch::Make(Match));
						bListNeedsUpdate = true;
					}
				}
			}

			// Look at running instances
			for (int32 i = 0; i < GameState->GameInstances.Num(); i++)
			{
				AUTLobbyMatchInfo* Match = GameState->GameInstances[i].MatchInfo;
				if (Match && GameState->IsMatchStillValid(Match) && Match->ShouldShowInDock())
				{
					int32 Idx = IsTrackingMatch(Match);
					if (Idx != INDEX_NONE)
					{
						TrackedMatches[Idx]->bPendingKill = false;
					}
					else
					{
						// We need to add this match
						TrackedMatches.Add(FTrackedMatch::Make(Match));
						bListNeedsUpdate = true;
					}
				}
			}
	
		}

		// Remove any entries that are pending kill
		for (int32 i = TrackedMatches.Num() - 1; i >= 0; i--)
		{
			if (TrackedMatches[i]->bPendingKill) 
			{
				bListNeedsUpdate = true;
				TrackedMatches.RemoveAt(i);
			}
		}

		// Update the friends.
		TArray<FUTFriend> FriendsList;
		PlayerOwner->GetFriendsList(FriendsList);

		for (int32 i=0; i < TrackedMatches.Num(); i++)
		{
			if (TrackedMatches[i]->MatchInfo.IsValid())
			{
				TrackedMatches[i]->NumFriends = TrackedMatches[i]->MatchInfo->CountFriendsInMatch(FriendsList);
			}
		}

		if (bShowingNoMatches && TrackedMatches.Num() > 0)
		{
			bShowingNoMatches = false;
			NoMatchesBox->ClearChildren();
		}
		else if (TrackedMatches.Num() == 0 && !bShowingNoMatches)
		{
			bShowingNoMatches = true;
			NoMatchesBox->AddSlot().AutoHeight().Padding(10.0f,10.0f,0.0f,0.0f)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUTMatchPanel","NoMatches","There are no active matches.  Start one!"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
			];
		}

		if (bListNeedsUpdate)
		{
			MatchList->RequestListRefresh();	
		}
	}
}
 

FReply SUTMatchPanel::StartNewMatch()
{
	AUTLobbyGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GameState && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
	{
		AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (LobbyPlayerState)
		{
			LobbyPlayerState->ServerCreateMatch();
		}
	}
	return FReply::Handled();
}	

TSharedRef<SWidget> SUTMatchPanel::OnGetPopupContent(TSharedPtr<SUTPopOverAnchor> Anchor, TSharedPtr<FTrackedMatch> TrackedMatch)
{
	CurrentAnchor = Anchor;

	// Create the player list..

	TSharedPtr<SVerticalBox> VertBox;
	SAssignNew(VertBox,SVerticalBox);

	TArray<FMatchPlayerListStruct> ColumnA;
	TArray<FMatchPlayerListStruct> ColumnB;
	FString Spectators;

	TArray<int32> Scores;
	float GameTime = 0.0f;

	// Holds a list of mutators running on this map.
	FString RulesList = TEXT("");

	bool bTeamGame = false;

	if (ShouldUseLiveData())
	{
		if (TrackedMatch->MatchInfo.IsValid())
		{
			TrackedMatch->MatchInfo->FillPlayerColumnsForDisplay(ColumnA, ColumnB, Spectators);
			if (TrackedMatch->MatchInfo->CurrentRuleset.IsValid())
			{
				bTeamGame = TrackedMatch->MatchInfo->CurrentRuleset->bTeamGame;
				RulesList = TrackedMatch->MatchInfo->CurrentRuleset->Description;
			}
			else if (TrackedMatch->MatchInfo->bDedicatedMatch)
			{
				bTeamGame = TrackedMatch->MatchInfo->bDedicatedTeamGame;
				RulesList = TrackedMatch->MatchInfo->DedicatedServerDescription;
			}

			Scores = TrackedMatch->MatchInfo->MatchUpdate.TeamScores;
			GameTime = TrackedMatch->MatchInfo->MatchUpdate.GameTime;
		}
	}
	else
	{
		TSharedPtr<FServerInstanceData> Instance;
		// Find the instance data
		for (int32 i=0; i < ServerData->HUBInstances.Num(); i++)
		{
			if (ServerData->HUBInstances[i]->InstanceId == TrackedMatch->MatchId)
			{
				Instance = ServerData->HUBInstances[i];
				break;
			}
		}
	
		if (!Instance.IsValid())
		{
			return SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(0.0,0.0,0.0,5.0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(0.0,0.0,5.0,0.0)
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(200)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().HAlign(HAlign_Left).AutoHeight().Padding(5.0f,5.0f,5.0f,5.0f)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTMatchPanel","NoData","Loading...."))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
						]
					]
				]
			];
		}
		else
		{
			Instance->Players.Sort(FMatchComparePlayersByScore());
			for (int32 i = 0; i < Instance->Players.Num(); i++)
			{
				if (Instance->bTeamGame)
				{
					if (Instance->Players[i].TeamNum == 0) ColumnA.Add(Instance->Players[i]);
					else if (Instance->Players[i].TeamNum == 1) ColumnB.Add(Instance->Players[i]);
				}
				else
				{
					if (i%2==0)
					{
						ColumnA.Add(Instance->Players[i]);
					}
					else
					{
						ColumnB.Add(Instance->Players[i]);
					}
				}
			}
			bTeamGame = Instance->bTeamGame;
			RulesList = Instance->MutatorList;
	
			Scores = Instance->MatchData.TeamScores;
			GameTime = Instance->MatchData.GameTime;
		}
	}

	// Generate a info widget from the UTLobbyMatchInfo

	int32 Max = FMath::Max<int32>(ColumnA.Num(), ColumnB.Num());

	// First we add the player list...

	if (bTeamGame)
	{
		VertBox->AddSlot()
		.Padding(0.0,0.0,0.0,5.0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0.0,0.0,5.0,0.0)
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(200)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().HAlign(HAlign_Left).AutoHeight().Padding(5.0f,0.0f,0.0f,0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUTMatchPanel","RedTeam","Red Team"))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
					]
				]
			]
			+SHorizontalBox::Slot()
			.Padding(5.0,0.0,0.0,0.0)
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(SBox).WidthOverride(200)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().HAlign(HAlign_Right).AutoHeight().Padding(0.0f,0.0f,5.0f,0.0f)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUTMatchPanel","BlueTeam","Blue Team"))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
					]
				]
			]
		];
	}
	else
	{
		VertBox->AddSlot()
		.Padding(0.0,0.0,0.0,5.0)
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUTMatchPanel","Players","Players in Match"))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
		];
	}

	TSharedPtr<SBox> ListBox;
	TSharedPtr<SVerticalBox> ListVBox;
	SAssignNew(ListBox, SBox).MinDesiredHeight(150)
	[
		SAssignNew(ListVBox, SVerticalBox)				
	];

	if (Max > 0)
	{
		for (int32 i = 0; i < Max; i++)
		{
			ListVBox->AddSlot()
			.AutoHeight()
			.Padding(0.0,0.0,0.0,2.0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(0.0,0.0,5.0,0.0)
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(200)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush( (i%2==0 ? FName(TEXT("UT.ListBackground.Even")) : FName(TEXT("UT.ListBackground.Odd")))  ))
						]
						+SOverlay::Slot()
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot().HAlign(HAlign_Left).AutoHeight()
								.Padding(5.0,0.0,0.0,0.0)
								[
									SNew(STextBlock)
									.Text(FText::FromString( (i < ColumnA.Num() ? ColumnA[i].PlayerName : TEXT(""))  ))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]

							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot().HAlign(HAlign_Right).AutoHeight()
								.Padding(0.0,0.0,5.0,0.0)
								[
									SNew(STextBlock)
									.Text(FText::FromString( (i < ColumnA.Num() ? ColumnA[i].PlayerScore : TEXT(""))  ))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]

							]
						]
					]
				]
				+SHorizontalBox::Slot()
				.Padding(5.0,0.0,0.0,0.0)
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(200)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush( (i%2==0 ? FName(TEXT("UT.ListBackground.Even")) : FName(TEXT("UT.ListBackground.Odd")))  ))
						]
						+SOverlay::Slot()
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot().HAlign(HAlign_Left).AutoHeight()
								.Padding(5.0,0.0,0.0,0.0)
								[
									SNew(STextBlock)
									.Text(FText::FromString( (i < ColumnB.Num() ? ColumnB[i].PlayerName : TEXT(""))  ))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]

							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot().HAlign(HAlign_Right).AutoHeight()
								.Padding(0.0,0.0,5.0,0.0)
								[
									SNew(STextBlock)
									.Text(FText::FromString( (i < ColumnB.Num() ? ColumnB[i].PlayerScore : TEXT(""))  ))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]
							]
						]
					]
				]
			];
		}
	}
	else
	{
		ListVBox->AddSlot()
		.AutoHeight()
		.Padding(0.0,0.0,0.0,2.0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(0.0,0.0,5.0,0.0)
			.FillWidth(1.0)
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().HAlign(HAlign_Left).AutoHeight()
				.Padding(5.0,0.0,0.0,0.0)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTMatchPanel","NoPlayers","No one is playing, join now!"))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
				]
			]
		];
	}

	VertBox->AddSlot().AutoHeight()
	[
		ListBox.ToSharedRef()
	];

	if (!Spectators.IsEmpty())
	{
		// Add Spectators

		VertBox->AddSlot()
		.HAlign(HAlign_Center)
		.Padding(0.0f,10.0f,0.0f,5.0)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUTMatchPanel","Spectators","Spectators"))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
		];

		VertBox->AddSlot()
		.HAlign(HAlign_Center)
		.Padding(5.0f,0.0f,5.0f,5.0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Spectators))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny")
				.AutoWrapText(true)
			]
		];
	}


	if ( TrackedMatch.IsValid() && TrackedMatch->MatchData.IsValid() )
	{
		VertBox->AddSlot()
		.HAlign(HAlign_Center)
		.Padding(5.0f,0.0f,5.0f,5.0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TrackedMatch->MatchData->MatchData.MatchState.ToString()))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny")
				.AutoWrapText(true)
			]
		];
	}

	if (Scores.Num() == 2)
	{
		FText ScoreText = FText::Format( NSLOCTEXT("SUTMatchPanel","ScoreTextFormat","<UT.Font.TeamScore.Red>{0}</> - <UT.Font.TeamScore.Blue>{1}</>"), FText::AsNumber(Scores[0]), FText::AsNumber(Scores[1]) );
		VertBox->AddSlot()
		.AutoHeight()
		.Padding(0.0,10.0,0.0,0.0)
		.HAlign(HAlign_Center)
		[
			SNew(SRichTextBlock)
			.Text(ScoreText)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
			.Justification(ETextJustify::Center)
			.DecoratorStyleSet(&SUTStyle::Get())
			.AutoWrapText(false)
		];
	}

	VertBox->AddSlot()
	.Padding(0.0,0.0,0.0,10.0)
	.AutoHeight()
	.HAlign(HAlign_Center)
	[
		SNew(STextBlock)
		.Text(UUTGameEngine::ConvertTime(FText::GetEmpty(), FText::GetEmpty(), GameTime, true, true, true))
		.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
	];



	if (!RulesList.IsEmpty())
	{
		VertBox->AddSlot().AutoHeight().Padding(0.0,0.0,0.0,5.0).HAlign(HAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperLight"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTMatchPanel","RulesTitle","Game Rules"))
					.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Tiny.Bold")
				]
			]
		];

		VertBox->AddSlot().AutoHeight().Padding(5.0,0.0,5.0,5.0)
		[
			SNew(SRichTextBlock)
			.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Tiny")
			.Justification(ETextJustify::Left)
			.DecoratorStyleSet( &SUWindowsStyle::Get() )
			.AutoWrapText( true )
			.Text(FText::FromString(RulesList))
		];
	}
	return VertBox.ToSharedRef();

}

TSharedRef<SWidget> SUTMatchPanel::OnGetPopup(TSharedPtr<SUTPopOverAnchor> Anchor, TSharedPtr<FTrackedMatch> TrackedMatch)
{

	if (bSuspendPopups)
	{
		return SNew(SCanvas);
	}

	return SNew(SBox).WidthOverride(420)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.0,5.0,5.0,5.0)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Medium"))
				]
				+SOverlay::Slot()
				[
					OnGetPopupContent(Anchor, TrackedMatch)
				]
			]
		];
}

void SUTMatchPanel::OnListMouseButtonDoubleClick(TSharedPtr<FTrackedMatch> SelectedMatch)
{
	if (!SelectedMatch->CanJoin()) return;

	bSuspendPopups = true;

	if (ShouldUseLiveData())
	{
		AUTLobbyGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>();
		if (SelectedMatch.IsValid() && GameState && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
		{
			AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
			if (LobbyPlayerState)
			{
				LobbyPlayerState->ServerJoinMatch(SelectedMatch->MatchInfo.Get(), false);
			}
		}
	}
	else
	{
		if (OnJoinMatchDelegate.IsBound())
		{
			OnJoinMatchDelegate.Execute(SelectedMatch->MatchId.ToString(), false);
		}
	}
}



FReply SUTMatchPanel::JoinMatchButtonClicked(TSharedPtr<FTrackedMatch> InItem)
{
	if (InItem.IsValid() && InItem->CanJoin() )
	{
		bSuspendPopups = true;
		if (ShouldUseLiveData())
		{
			if (InItem.IsValid() && InItem->MatchInfo.IsValid())
			{
				if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
				{
					AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
					if (LobbyPlayerState)
					{
						LobbyPlayerState->ServerJoinMatch(InItem->MatchInfo.Get(),false);
					}
				}
			}
		}
		else
		{
			if (OnJoinMatchDelegate.IsBound())
			{
				OnJoinMatchDelegate.Execute(InItem->MatchId.ToString(), false);
			}
		}
	}
	return FReply::Handled();
}
FReply SUTMatchPanel::SpectateMatchButtonClicked(TSharedPtr<FTrackedMatch> InItem)
{
	if (InItem.IsValid() && InItem->CanSpectate())
	{
		bSuspendPopups = true;
		if (ShouldUseLiveData())
		{
			if (InItem.IsValid() && InItem->MatchInfo.IsValid())
			{
				if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
				{
					AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
					if (LobbyPlayerState)
					{
						LobbyPlayerState->ServerJoinMatch(InItem->MatchInfo.Get(),true);
					}
				}
			}	
		}
		else
		{
			if (OnJoinMatchDelegate.IsBound())
			{
				OnJoinMatchDelegate.Execute(InItem->MatchId.ToString(), true);
			}
		}
	}

	return FReply::Handled();
}


void SUTMatchPanel::SetServerData(TSharedPtr<FServerData> inServerData)
{
	TrackedMatches.Empty();

	// Update the friends.
	TArray<FUTFriend> FriendsList;
	PlayerOwner->GetFriendsList(FriendsList);

	ServerData = inServerData;
	for (int32 i=0; i < ServerData->HUBInstances.Num(); i++)
	{
		TSharedPtr<FServerInstanceData> Instance = ServerData->HUBInstances[i];
		if (Instance.IsValid())
		{
			int32 Index = TrackedMatches.Add(FTrackedMatch::Make(Instance));

			// Count the # of friends....
			int32 NumFriends = 0;
			for (int32 j=0; j < Instance->Players.Num(); j++)
			{
				for (int32 k=0; k < FriendsList.Num(); k++)
				{
					if (FriendsList[k].UserId == Instance->Players[j].PlayerId)
					{
						NumFriends++;
						break;
					}
				}
			}

			TrackedMatches[Index]->NumFriends = NumFriends;
		}
	}
	MatchList->RequestListRefresh();
}

FReply SUTMatchPanel::DownloadAllButtonClicked()
{
	PlayerOwner->DownloadAll();
	return FReply::Handled();
}


#endif