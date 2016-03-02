// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTPartyWidget.h"
#include "BlueprintContextLibrary.h"
#include "PartyContext.h"
#include "SUTComboButton.h"
#include "UTParty.h"
#include "UTGameInstance.h"
#include "../SUTStyle.h"

#if !UE_SERVER

void SUTPartyWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx)
{
	Ctx = InCtx;
	
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
	
	TArray<FText> PartyMembers;
	PartyContext->GetLocalPartyMemberNames(PartyMembers);

	if (PartyMembers.Num() <= 1)
	{
		return;
	}

	TArray<FUniqueNetIdRepl> PartyMemberIds;
	PartyContext->GetLocalPartyMemberIDs(PartyMemberIds);

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
		
		FSlateColor PartyMemberColor = FLinearColor::White;
		if (PartyMemberIds[i] == PartyLeaderId)
		{
			if (LocalPlayerId == PartyLeaderId)
			{
				PartyMemberColor = FLinearColor::Green;
			}
			else
			{
				PartyMemberColor = FLinearColor::Blue;
			}
		}

		PartyMemberBox->AddSlot()
		[
			SAssignNew(DropDownButton, SUTComboButton)
			.HasDownArrow(false)
			.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
			.ContentPadding(FMargin(0.0f, 0.0f))
			.ToolTipText(PartyMembers[i])
			.ButtonContent()
			[
				SNew(SBox)
				.WidthOverride(96)
				.HeightOverride(96)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Icon.PlayerCard"))
					.ColorAndOpacity(PartyMemberColor)
				]
			]
		];

		if (LocalPlayerId == PartyLeaderId && PartyMemberIds[i] != PartyLeaderId)
		{
			DropDownButton->AddSubMenuItem(PartyMembers[i], FOnClicked::CreateSP(this, &SUTPartyWidget::PlayerNameClicked, i));
			DropDownButton->AddSpacer();
			DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "PromoteToLeader", "Promote To Leader"), FOnClicked::CreateSP(this, &SUTPartyWidget::PromoteToLeader, i));
			DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTPartyWidget", "KickFromParty", "Kick From Party"), FOnClicked::CreateSP(this, &SUTPartyWidget::KickFromParty, i), true);
		}
		else
		{
			DropDownButton->AddSubMenuItem(PartyMembers[i], FOnClicked::CreateSP(this, &SUTPartyWidget::PlayerNameClicked, i), true);
		}
	}
}

FReply SUTPartyWidget::PlayerNameClicked(int32 PartyMemberIdx)
{
	return FReply::Handled();
}

FReply SUTPartyWidget::KickFromParty(int32 PartyMemberIdx)
{
	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));
	if (PartyContext)
	{
		TArray<FUniqueNetIdRepl> PartyMemberIds;
		PartyContext->GetLocalPartyMemberIDs(PartyMemberIds);
		PartyContext->KickPartyMember(PartyMemberIds[PartyMemberIdx]);
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
		PartyContext->PromotePartyMemberToLeader(PartyMemberIds[PartyMemberIdx]);
	}

	return FReply::Handled();
}

SUTPartyWidget::~SUTPartyWidget()
{
}

#endif