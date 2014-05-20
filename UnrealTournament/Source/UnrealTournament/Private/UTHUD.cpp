// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD.h"

AUTHUD::AUTHUD(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("/Game/Textures/Crosshair"));
	CrosshairTex = CrosshairTexObj.Object;
}

void AUTHUD::ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject)
{
}

void AUTHUD::DrawHUD()
{
	Super::DrawHUD();

	// Draw very simple crosshair

	// find center of the Canvas
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// offset by half the texture's dimensions so that the center of the texture aligns with the center of the Canvas
	const FVector2D CrosshairDrawPosition( (Center.X - (CrosshairTex->GetSurfaceWidth() * 0.5f)),
										   (Center.Y - (CrosshairTex->GetSurfaceHeight() * 0.5f)) );

	// draw the crosshair
	FCanvasTileItem TileItem( CrosshairDrawPosition, CrosshairTex->Resource, FLinearColor::White);
	TileItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem( TileItem );

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		TempDrawString(FText::FromString(TEXT("Alpha Prototype")), Center.X, 5.0f, ETextHorzPos::Center, GEngine->GetSmallFont(), FLinearColor::White);
		TempDrawString( TempConvertTime(GameState->ElapsedTime), Center.X, 20, ETextHorzPos::Center, GEngine->GetMediumFont(), FLinearColor::White);
	}
}

FText AUTHUD::TempConvertTime(int Seconds)
{
	int Hours = Seconds / 3600;
	Seconds -= Hours * 3600;
	int Mins = Seconds / 60;
	Seconds -= Mins * 60;

	FFormatNamedArguments Args;
	FNumberFormattingOptions Options;

	Options.MinimumIntegralDigits = 2;
	Options.MaximumIntegralDigits = 2;

	Args.Add(TEXT("Hours"), FText::AsNumber(Hours, &Options));
	Args.Add(TEXT("Minutes"), FText::AsNumber(Mins, &Options));
	Args.Add(TEXT("Seconds"), FText::AsNumber(Seconds, &Options));

	return FText::Format( NSLOCTEXT("UTHUD","TIMERHOURS", "{Hours}:{Minutes}:{Seconds}"),Args);
}


void AUTHUD::TempDrawString(FText Text, float X, float Y, ETextHorzPos::Type TextPosition, UFont* Font, FLinearColor Color)
{

	FVector2D RenderPos = FVector2D(X,Y);
	if (TextPosition != ETextHorzPos::Left)
	{
		int32 XL, YL;
		Font->GetStringHeightAndWidth(Text.ToString(), YL, XL);
		RenderPos.X -= TextPosition == ETextHorzPos::Right ? XL : XL * 0.5f;
	}

	FCanvasTextItem TextItem(RenderPos, Text, Font, Color);
	Canvas->DrawItem(TextItem);
}