// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_Gauntlet.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlag.h"

AUTHUD_Gauntlet::AUTHUD_Gauntlet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTHUD_Gauntlet::DrawMinimapSpectatorIcons()
{
	AUTGauntletGameState* GS = Cast<AUTGauntletGameState>(GetWorld()->GetGameState());
	if (GS == NULL) return;


	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner);
	if (PC == nullptr) return;

	const float RenderScale = float(Canvas->SizeY) / 1080.0f;

	if (GS && GS->Flag && GS->Flag->GetTeamNum() != 1 - PC->GetTeamNum())
	{
		FVector2D Pos = WorldToMapToScreen(GS->Flag->GetActorLocation());
		Canvas->DrawColor = FColor(0, 200, 0, 255);
		DrawMinimapIcon(HUDAtlas, Pos, FVector2D(30.f, 30.f), FVector2D(843.f, 87.f), FVector2D(43.f, 41.f), FColor(0,200,0,255), true);
	}

	Super::DrawMinimapSpectatorIcons();
}