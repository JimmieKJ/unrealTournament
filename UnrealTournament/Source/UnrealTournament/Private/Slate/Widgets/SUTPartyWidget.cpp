// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTPartyWidget.h"
#include "BlueprintContextLibrary.h"
#include "PartyContext.h"
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
	PartyContext->OnPartyJoined.AddSP(this, &SUTPartyWidget::PartyJoined);
	PartyContext->OnPartyLeft.AddSP(this, &SUTPartyWidget::PartyLeft);
}

void SUTPartyWidget::PartyJoined()
{
	SetupPartyMemberBox();
}

void SUTPartyWidget::PartyLeft()
{
	SetupPartyMemberBox();
}

void SUTPartyWidget::SetupPartyMemberBox()
{
	PartyMemberBox->ClearChildren();

	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(Ctx.GetWorld(), UPartyContext::StaticClass()));
	
	TArray<FText> PartyMembers;
	PartyContext->GetLocalPartyMemberNames(PartyMembers);

	for (int i = 0; i < PartyMembers.Num(); i++)
	{
		PartyMemberBox->AddSlot()
		[
			SNew(SBox).WidthOverride(128).HeightOverride(128)
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.Icon.PlayerCard"))
			]
		];
	}
}

SUTPartyWidget::~SUTPartyWidget()
{
}

#endif