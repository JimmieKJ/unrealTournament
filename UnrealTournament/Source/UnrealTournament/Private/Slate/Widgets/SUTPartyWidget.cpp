// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTPartyWidget.h"
#include "BlueprintContextLibrary.h"
#include "MatchmakingContext.h"
#include "PartyContext.h"
#include "SUTComboButton.h"
#include "UTParty.h"
#include "UTGameInstance.h"
#include "../SUTStyle.h"

#if WITH_SOCIAL
#include "Social.h"
#endif

#if !UE_SERVER

void SUTPartyWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	Ctx = InCtx;

	RefreshTimer = 30.0f;
	LastFriendCount = 0;

	bPartyMenuCollapsed = true;
	
	ChildSlot
	.HAlign(HAlign_Right)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor::Gray) // Darken the outer border
		[
			SAssignNew(PartyMemberBox, SHorizontalBox)
		]
	];

	SetupPartyMemberBox();

	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));
	if (PartyContext)
	{
		PartyContext->OnPlayerStateChanged.AddSP(this, &SUTPartyWidget::PartyStateChanged);
		PartyContext->OnPartyLeft.AddSP(this, &SUTPartyWidget::PartyLeft);
		PartyContext->OnPartyMemberPromoted.AddSP(this, &SUTPartyWidget::PartyMemberPromoted);
	}
}


void SUTPartyWidget::PartyMemberPromoted()
{
	SetupPartyMemberBox();
}

void SUTPartyWidget::PartyLeft()
{
	SetupPartyMemberBox();
}

void SUTPartyWidget::PartyStateChanged()
{
	SetupPartyMemberBox();
}


void SUTPartyWidget::SetupPartyMemberBox()
{
	PartyMemberBox->ClearChildren();
	
	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));

	const int32 PartySize = PartyContext->GetPartySize();
	if (PartySize < 1)
	{
		return;
	}

	if (PartySize >= 2)
	{
		bPartyMenuCollapsed = false;
	}
	
	TArray<FText> PartyMembers;
	PartyContext->GetLocalPartyMemberNames(PartyMembers);
	
	TArray<FUniqueNetIdRepl> PartyMemberIds;
	PartyContext->GetLocalPartyMemberIDs(PartyMemberIds);

	EPartyType PartyType = EPartyType::Public;
	bool bLeaderInvitesOnly = false;
	bool bLeaderFriendsOnly = false;

	TSharedPtr<const FUniqueNetId> PartyLeaderId = nullptr;
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface();
	if (PartyInt.IsValid())
	{
		UUTGameInstance* GameInstance = CastChecked<UUTGameInstance>(Ctx.GetPlayerController()->GetGameInstance());
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UPartyGameState* PersistentParty = Party->GetPersistentParty();
			if (PersistentParty)
			{
				PartyLeaderId = PersistentParty->GetPartyLeader();

				PartyType = PersistentParty->GetPartyType();
				bLeaderInvitesOnly = PersistentParty->IsLeaderInvitesOnly();
				bLeaderFriendsOnly = PersistentParty->IsLeaderFriendsOnly();
			}
		}
	}

	FUniqueNetIdRepl LocalPlayerId;
	UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(Ctx.GetLocalPlayer());
	if (UTLP)
	{
		LocalPlayerId = UTLP->GetGameAccountId();
	}

	for (int i = 0; i < PartyMembers.Num(); i++)
	{
		TSharedPtr<SUTComboButton> DropDownButton = NULL;

		if (PartyMemberIds[i] == PartyLeaderId)
		{	
			PartyMemberBox->AddSlot()
			.Padding(FMargin(2.0f, 0.0f))
			[
				SAssignNew(DropDownButton, SUTComboButton)
				.HasDownArrow(false)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(0.0f, 0.0f))
				.ToolTipText(PartyMembers[i])
				.OnSubmenuShown(this, &SUTPartyWidget::LeaderButtonClicked)
				.ButtonContent()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBox)
						.WidthOverride(128)
						.HeightOverride(128)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Icon.PartyLeader"))
						]
					]
					+ SOverlay::Slot()
					[
						SNew(SBox)
						.WidthOverride(128)
						.HeightOverride(128)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUTPartyWidget", "PartyTitle", "Party"))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
								.Visibility(bPartyMenuCollapsed ? EVisibility::Visible : EVisibility::Collapsed)
							]
							+SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Bottom)
							.Padding(0, 10)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
								]
								+ SOverlay::Slot()
								[
									SNew(STextBlock)
									.Text(PartyMembers[i])
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
									.Visibility(bPartyMenuCollapsed ? EVisibility::Collapsed : EVisibility::Visible)
								]
							]
						]
					]
				]
			];

			DropDownButton->AddSubMenuItem(PartyMembers[i], FOnClicked::CreateSP(this, &SUTPartyWidget::PlayerNameClicked, i));
			
			if (LocalPlayerId == PartyLeaderId)
			{
				DropDownButton->AddSpacer();

				if (PartyType == EPartyType::Public)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "PublicParty", "[Public Party]"), FOnClicked());
				}
				else if (PartyType == EPartyType::FriendsOnly)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "FriendsOnlyParty", "[Friends Only Party]"), FOnClicked());
				}
				else if (PartyType == EPartyType::Private)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "PrivateParty", "[Private Party]"), FOnClicked());
				}

				if (bLeaderInvitesOnly || bLeaderFriendsOnly)
				{
					DropDownButton->AddSpacer();
				}

				if (bLeaderInvitesOnly)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "LeaderInvitesOnly", "[Only Leader Invites]"), FOnClicked());
				}

				if (bLeaderFriendsOnly)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "LeaderFriendsOnly", "[Only Leader Friends]"), FOnClicked());
				}

				DropDownButton->AddSpacer();

				if (PartyType != EPartyType::Public)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "MakePartyPublic", "Make Party Public"), FOnClicked::CreateSP(this, &SUTPartyWidget::ChangePartyType, EPartyType::Public));
				}
				if (PartyType != EPartyType::FriendsOnly)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "MakePartyFriendsOnly", "Make Party Friends Only"), FOnClicked::CreateSP(this, &SUTPartyWidget::ChangePartyType, EPartyType::FriendsOnly));
				}
				if (PartyType != EPartyType::Private)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "MakePartyPrivate", "Make Party Private"), FOnClicked::CreateSP(this, &SUTPartyWidget::ChangePartyType, EPartyType::Private));
				}
				// These don't seem to work, so disable for now
#if 0
				if (bLeaderInvitesOnly)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "UndoLeaderInvitesOnly", "Allow Member Invites"), FOnClicked::CreateSP(this, &SUTPartyWidget::AllowMemberInvites, true));
				}
				else
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "MakeLeaderInvitesOnly", "Disallow Member Invites"), FOnClicked::CreateSP(this, &SUTPartyWidget::AllowMemberInvites, false));
				}

				if (bLeaderFriendsOnly)
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "UndoLeaderFriendsOnly", "Allow Member Friends"), FOnClicked::CreateSP(this, &SUTPartyWidget::AllowMemberFriends, true));
				}
				else
				{
					DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "MakeLeaderFriendsOnly", "Disallow Member Friends"), FOnClicked::CreateSP(this, &SUTPartyWidget::AllowMemberFriends, false));
				}
#endif
			}

			DropDownButton->RebuildMenuContent();
		}
	}

	for (int i = 0; i < PartyMembers.Num(); i++)
	{
		TSharedPtr<SUTComboButton> DropDownButton = NULL;

		if (PartyMemberIds[i] != PartyLeaderId)
		{
			PartyMemberBox->AddSlot()
			.Padding(FMargin(2.0f, 0.0f))
			[
				SAssignNew(DropDownButton, SUTComboButton)
				.HasDownArrow(false)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(0.0f, 0.0f))
				.ToolTipText(PartyMembers[i])
				.ButtonContent()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBox)
						.WidthOverride(128)
						.HeightOverride(128)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Icon.PartyMember"))
						]
					]
					+ SOverlay::Slot()
					[
						SNew(SBox)
						.WidthOverride(128)
						.HeightOverride(128)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Bottom)
							.Padding(0, 10)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
								]
								+ SOverlay::Slot()
								[
									SNew(STextBlock)
									.Text(PartyMembers[i])
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tiny.Bold")
									.Visibility(bPartyMenuCollapsed ? EVisibility::Collapsed : EVisibility::Visible)
								]
							]
						]
					]
				]
			];

			DropDownButton->AddSubMenuItem(PartyMembers[i], FOnClicked::CreateSP(this, &SUTPartyWidget::PlayerNameClicked, i));

			if (LocalPlayerId == PartyMemberIds[i])
			{
				DropDownButton->AddSpacer();
				DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "LeaveParty", "Leave Party"), FOnClicked::CreateSP(this, &SUTPartyWidget::LeaveParty));
			}
			
			if (LocalPlayerId == PartyLeaderId)
			{
				DropDownButton->AddSpacer();
				DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "PromoteToLeader", "Promote To Leader"), FOnClicked::CreateSP(this, &SUTPartyWidget::PromoteToLeader, i));
				DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "KickFromParty", "Kick From Party"), FOnClicked::CreateSP(this, &SUTPartyWidget::KickFromParty, i));
			}

			DropDownButton->RebuildMenuContent();
		}
	}
	
	if (bPartyMenuCollapsed)
	{
		return;
	}

	TArray<FUTFriend> OnlineFriendsList;
	UTLP->GetFriendsList(OnlineFriendsList);
	LastFriendCount = 0;

	for (int i = PartyMembers.Num(); i < 5; i++)
	{
		TSharedPtr<SUTComboButton> DropDownButton = NULL;

		PartyMemberBox->AddSlot()
		.Padding(FMargin(2.0f, 0.0f))
		[
			SAssignNew(DropDownButton, SUTComboButton)
			.HasDownArrow(false)
			.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
			.ContentPadding(FMargin(0.0f, 0.0f))
			.ToolTipText(NSLOCTEXT("SUTPartyWidget", "EmptySlotTip", "Available Slot"))
			.ButtonContent()
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.WidthOverride(128)
					.HeightOverride(128)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.Icon.PartyMember.Empty"))
						.ColorAndOpacity(FLinearColor::Gray)
					]
				]
			]
		];

		DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "EmptySlot", "Available Slot"), FOnClicked());

		int InviteTextAdded = 0;
		for (int FriendIterator = 0; FriendIterator < OnlineFriendsList.Num(); FriendIterator++)
		{
			if (OnlineFriendsList[FriendIterator].bIsOnline && OnlineFriendsList[FriendIterator].bIsPlayingThisGame)
			{
				LastFriendCount++;
				if (InviteTextAdded == 0)
				{
					DropDownButton->AddSpacer();
				}
				FText FriendText = FText::Format(NSLOCTEXT("SUTPartyWidget", "InviteFriend", "Invite {0}"), FText::FromString(OnlineFriendsList[FriendIterator].DisplayName));
				DropDownButton->AddSubMenuItem(FriendText, FOnClicked::CreateSP(this, &SUTPartyWidget::InviteToParty, OnlineFriendsList[FriendIterator].UserId));
				InviteTextAdded++;
			}
		}
		DropDownButton->RebuildMenuContent();
	}
}

FReply SUTPartyWidget::InviteToParty(FString UserId)
{
#if WITH_SOCIAL
	TSharedPtr<IGameAndPartyService> GameAndPartyService = ISocialModule::Get().GetFriendsAndChatManager(TEXT(""), true)->GetGameAndPartyService();
	TSharedPtr<const FUniqueNetId > UniqueNetId = MakeShareable(new FUniqueNetIdString(*UserId));
	GameAndPartyService->SendPartyInvite(*UniqueNetId);
#endif

	return FReply::Handled();
}

FReply SUTPartyWidget::PlayerNameClicked(int32 PartyMemberIdx)
{
	// Show player card or something?

	return FReply::Handled();
}

FReply SUTPartyWidget::LeaveParty()
{
	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));
	if (PartyContext)
	{
		PartyContext->LeaveParty();
	}

	return FReply::Handled();
}

FReply SUTPartyWidget::KickFromParty(int32 PartyMemberIdx)
{
	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));
	if (PartyContext)
	{
		TArray<FUniqueNetIdRepl> PartyMemberIds;
		PartyContext->GetLocalPartyMemberIDs(PartyMemberIds);
		if (PartyMemberIds.IsValidIndex(PartyMemberIdx))
		{
			PartyContext->KickPartyMember(PartyMemberIds[PartyMemberIdx]);
		}
	}

	return FReply::Handled();
}

FReply SUTPartyWidget::PromoteToLeader(int32 PartyMemberIdx)
{
	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));
	if (PartyContext)
	{
		TArray<FUniqueNetIdRepl> PartyMemberIds;
		PartyContext->GetLocalPartyMemberIDs(PartyMemberIds);
		if (PartyMemberIds.IsValidIndex(PartyMemberIdx))
		{
			PartyContext->PromotePartyMemberToLeader(PartyMemberIds[PartyMemberIdx]);
		}
	}

	return FReply::Handled();
}

FReply SUTPartyWidget::ChangePartyType(EPartyType InNewPartyType)
{
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface();
	if (PartyInt.IsValid())
	{
		UUTGameInstance* GameInstance = CastChecked<UUTGameInstance>(Ctx.GetPlayerController()->GetGameInstance());
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UPartyGameState* PersistentParty = Party->GetPersistentParty();
			if (PersistentParty)
			{
				PersistentParty->SetPartyType(InNewPartyType, PersistentParty->IsLeaderFriendsOnly(), PersistentParty->IsLeaderInvitesOnly());
			}
		}
	}

	SetupPartyMemberBox();

	return FReply::Handled();
}

void SUTPartyWidget::LeaderButtonClicked()
{
	if (bPartyMenuCollapsed)
	{
		bPartyMenuCollapsed = false;
		SetupPartyMemberBox();
	}
}

FReply SUTPartyWidget::AllowMemberFriends(bool bAllow)
{
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface();
	if (PartyInt.IsValid())
	{
		UUTGameInstance* GameInstance = CastChecked<UUTGameInstance>(Ctx.GetPlayerController()->GetGameInstance());
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UPartyGameState* PersistentParty = Party->GetPersistentParty();
			if (PersistentParty)
			{				
				PersistentParty->SetPartyType(PersistentParty->GetPartyType(), !bAllow, PersistentParty->IsLeaderInvitesOnly());
			}
		}
	}

	SetupPartyMemberBox();

	return FReply::Handled();
}

FReply SUTPartyWidget::AllowMemberInvites(bool bAllow)
{
	IOnlinePartyPtr PartyInt = Online::GetPartyInterface();
	if (PartyInt.IsValid())
	{
		UUTGameInstance* GameInstance = CastChecked<UUTGameInstance>(Ctx.GetPlayerController()->GetGameInstance());
		UUTParty* Party = GameInstance->GetParties();
		if (Party)
		{
			UPartyGameState* PersistentParty = Party->GetPersistentParty();
			if (PersistentParty)
			{
				PersistentParty->SetPartyType(PersistentParty->GetPartyType(), PersistentParty->IsLeaderFriendsOnly(), !bAllow);
			}
		}
	}

	SetupPartyMemberBox();

	return FReply::Handled();
}

void SUTPartyWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	RefreshTimer -= InDeltaTime;
	if (RefreshTimer < 0)
	{
		RefreshTimer = 30.0f;

		TArray<FUTFriend> OnlineFriendsList;
		UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(Ctx.GetLocalPlayer());
		if (UTLP)
		{
			UTLP->GetFriendsList(OnlineFriendsList);

			int32 OnlineFriendCount = 0;
			for (int FriendIterator = 0; FriendIterator < OnlineFriendsList.Num(); FriendIterator++)
			{
				if (OnlineFriendsList[FriendIterator].bIsOnline && OnlineFriendsList[FriendIterator].bIsPlayingThisGame)
				{
					OnlineFriendCount++;
				}
			}

			if (OnlineFriendCount != LastFriendCount)
			{
				SetupPartyMemberBox();
			}
		}
	}
}

#endif