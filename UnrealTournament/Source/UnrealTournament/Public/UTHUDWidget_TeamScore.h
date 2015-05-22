// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTHUDWidget_TeamScore.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTHUDWidget_TeamScore : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	UUTHUDWidget_TeamScore(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		ScreenPosition = FVector2D(0.5f, 0.0f);
		static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
		IconTexture = HudTexture.Object;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	UTexture* IconTexture;

	virtual void Draw_Implementation(float DeltaTime)
	{
		FLinearColor BGColor = ApplyHUDColor(FLinearColor::White);

		float RedScale  = UTHUDOwner->UTPlayerOwner->GetTeamNum() == 0 ? 1.25 : 1.0;
		float BlueScale = UTHUDOwner->UTPlayerOwner->GetTeamNum() == 1 ? 1.25 : 1.0;

		DrawTexture(IconTexture, -162 * RedScale - 15, 0, 162 * RedScale, 80 * RedScale, 1,0,108,53,1.0,BGColor);
		DrawTexture(IconTexture, -15,  0, 30, 80,  109,0,20,53,1.0,BGColor);
		DrawTexture(IconTexture, 15, 0, 162 * BlueScale, 80 * BlueScale, 129,0,108,53,1.0,BGColor);

		// Draw the Red Score...

		AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL && GS->Teams.Num() == 2 && GS->Teams[0] != NULL && GS->Teams[1] != NULL)
		{
			// Draw the Red Score...
			UTHUDOwner->DrawNumber(GS->Teams[0]->Score, RenderPosition.X - (157 * RenderScale * RedScale), 10 * RenderScale * RedScale, GS->Teams[0]->TeamColor, 1.0, RenderScale * RedScale * 0.75);
	
			// Draw the Blue Score...
			UTHUDOwner->DrawNumber(GS->Teams[1]->Score, RenderPosition.X + (75 * RenderScale * BlueScale), 10 * RenderScale * BlueScale, GS->Teams[1]->TeamColor, 1.0, RenderScale * BlueScale * 0.75);
		}
	}
};