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
		SAssignNew(PartyMemberBox, SHorizontalBox)
	];

	SetupPartyMemberBox();

	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));
	if (PartyContext)
	{
		PartyContext->OnPlayerStateChanged.AddSP(this, &SUTPartyWidget::PartyStateChanged);
	}
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
	return FReply::Handled();
}

FReply SUTPartyWidget::PromoteToLeader(int32 PartyMemberIdx)
{
	return FReply::Handled();
}

SUTPartyWidget::~SUTPartyWidget()
{
}

#endif