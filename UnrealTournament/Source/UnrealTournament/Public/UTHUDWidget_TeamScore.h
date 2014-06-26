// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTHUDWidget_TeamScore.generated.h"

UCLASS(CustomConstructor)
class UUTHUDWidget_TeamScore : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	UUTHUDWidget_TeamScore(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		ScreenPosition = FVector2D(0.5f, 0.0f);
		Origin = FVector2D(0.5f, 0.0f);
		Size = FVector2D(400.0f, 100.0f);
		static ConstructorHelpers::FObjectFinder<UTexture> TeamScoreTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
		TeamScoreBG.Texture = TeamScoreTexture.Object;
		TeamScoreBG.UL = 238;
		TeamScoreBG.VL = 52;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	FCanvasIcon TeamScoreBG;

	virtual void Draw_Implementation(float DeltaTime)
	{
		FLinearColor BGColor = FLinearColor::White;
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			BGColor = PS->Team->TeamColor;
		}
		
		DrawTexture(TeamScoreBG.Texture, 0.0, 0.0, GetRenderSize().X, GetRenderSize().Y, TeamScoreBG.U, TeamScoreBG.V, TeamScoreBG.UL, TeamScoreBG.VL, 1.0f, BGColor);

		AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			for (int32 i = 0; i < 2 && i < GS->Teams.Num(); i++)
			{
				if (GS->Teams[i] != NULL)
				{
					float XPos = 125.0f * GetRenderScale();
					if (i == 0)
					{
						XPos *= -1.0f;
					}
					DrawText(FText::AsNumber(GS->Teams[i]->Score), RenderSize.X * 0.5f + XPos, RenderSize.Y * 0.35f, UTHUDOwner->MediumFont, false, FVector2D(0.0f, 0.0f), FLinearColor::Black, false, FLinearColor::Black, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
				}
			}
		}
	}
};