// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_CastingGuide.h"
#include "UTHUDWidget.h"
#include "UTCarriedObject.h"

void AUTHUD_CastingGuide::DrawHUD()
{
	Super::DrawHUD();

	// figure out bind for this camera and draw it
	FString Binding;
	AActor* ViewTarget = PlayerOwner->GetViewTarget();
	APawn* PTarget = Cast<APawn>(ViewTarget);
	if (PTarget != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PTarget->PlayerState);
		if (PS != NULL)
		{
			if (PS->Team != NULL)
			{
				Binding = FString::Printf(TEXT("ViewPlayerNum %i %i"), int32(PS->SpectatingIDTeam), int32(PS->Team->TeamIndex));
			}
			else
			{
				Binding = FString::Printf(TEXT("ViewPlayerNum %i"), int32(PS->SpectatingID));
			}
		}
	}
	else
	{
		AUTCarriedObject* Flag = Cast<AUTCarriedObject>(ViewTarget);
		if (Flag != NULL)
		{
			Binding = FString::Printf(TEXT("ViewFlag %i"), int32(Flag->GetTeamNum()));
		}
	}
	if (!Binding.IsEmpty())
	{
		TArray<FKey> Keys;
		UTPlayerOwner->ResolveKeybindToFKey(Binding, Keys, true, false);
		FUTCanvasTextItem Item(FVector2D(10.0f, Canvas->ClipY * 0.9f), FText::Format(NSLOCTEXT("CastingGuide", "Bind", "Camera Bind: {0}"), FText::FromString((Keys.Num() > 0) ? Keys[0].ToString() : FString::Printf(TEXT("{%s}"), *Binding))), SmallFont, FLinearColor::White);
		Item.FontRenderInfo = Canvas->CreateFontRenderInfo(true, true);
		Canvas->DrawItem(Item);
	}
}