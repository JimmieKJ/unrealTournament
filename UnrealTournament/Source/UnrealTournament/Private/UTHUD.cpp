// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTHUDWidgetMessage.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTHUDWidgetMessage_DeathMessages.h"
#include "UTHUDWidget_DMPlayerLeaderboard.h"

AUTHUD::AUTHUD(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	WidgetOpacity = 1.0f;

	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("Texture2D'/Game/RestrictedAssets/Textures/crosshair.crosshair'"));
	CrosshairTex = CrosshairTexObj.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> OldHudTextureObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	OldHudTexture = OldHudTextureObj.Object;

	static ConstructorHelpers::FObjectFinder<UFont> MFont(TEXT("Font'/Game/RestrictedAssets/Proto/UI/Fonts/fntRobotoBlack36.fntRobotoBlack36'"));
	MediumFont = MFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> LFont(TEXT("Font'/Game/RestrictedAssets/Proto/UI/Fonts/fntRobotoBlack72.fntRobotoBlack72'"));
	LargeFont = LFont.Object;

	CrossHairCenterPoint = FVector2D(0.5f,0.5f);

}

void AUTHUD::BeginPlay()
{
	Super::BeginPlay();

	for (int WidgetIndex = 0 ; WidgetIndex < HudWidgetClasses.Num(); WidgetIndex++)
	{
		AddHudWidget(HudWidgetClasses[WidgetIndex]);
	}

	AddHudWidget(UUTHUDWidget_Paperdoll::StaticClass());
	AddHudWidget(UUTHUDWidget_DMPlayerScore::StaticClass());
	AddHudWidget(UUTHUDWidget_DMPlayerLeaderboard::StaticClass());
	AddHudWidget(UUTHUDWidgetMessage_DeathMessages::StaticClass());
}

void AUTHUD::AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass)
{
	UUTHUDWidget* Widget = ConstructObject<UUTHUDWidget>(NewWidgetClass,GetTransientPackage());
	HudWidgets.Add(Widget);

	UE_LOG(UT,Log,TEXT("Adding Widget %s"),*GetNameSafe(Widget));

	// If this widget is a messaging widget, then track it
	UUTHUDWidgetMessage* MessageWidget = Cast<UUTHUDWidgetMessage>(Widget);
	if (MessageWidget != NULL)
	{
		UE_LOG(UT,Log,TEXT(" -- Tracking as Message for Area %i"),*MessageWidget->ManagedMessageArea.ToString());
		HudMessageWidgets.Add(MessageWidget->ManagedMessageArea, MessageWidget);
	}

}

void AUTHUD::ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject)
{
	UE_LOG(UT,Log,TEXT("AUTHUD::ReceiveLocalMessage: %s %i %i"), *GetNameSafe(MessageClass), MessageIndex, *MessageClass->GetDefaultObject<UUTLocalMessage>()->MessageArea.ToString() );
	UUTHUDWidgetMessage* DestinationWidget = (HudMessageWidgets.FindRef(MessageClass->GetDefaultObject<UUTLocalMessage>()->MessageArea));
	if (DestinationWidget != NULL)
	{
		DestinationWidget->ReceiveLocalMessage(MessageClass, RelatedPlayerState_1, RelatedPlayerState_2,MessageIndex, LocalMessageText, OptionalObject);
	}
	else
	{
		UE_LOG(UT,Log,TEXT("No Message Widget to Display Text"));
	}
}

void AUTHUD::PostRender()
{
	// Always sort the PlayerState array at the beginning of each frame
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		GS->SortPRIArray();
	}
/* -- ENABLE to see the player list 

		float Y = Canvas->ClipY * 0.5;
		for (int i=0;i<GS->PlayerArray.Num();i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
			if (PS != NULL)
			{
				FString Text = FString::Printf(TEXT("%s - %i"), *PS->PlayerName, PS->Score);
				TempDrawString(FText::FromString(Text), 0, Y, ETextHorzPos::Left, GetFontFromSizeIndex(0), FLinearColor::White);
				Y+=24;			
			}
		}
	}
*/
	if (PlayerOwner != NULL)
	{
		UTPlayerOwner = Cast<AUTPlayerController>(PlayerOwner);
		if (UTPlayerOwner != NULL)
		{
			UTCharacterOwner = UTPlayerOwner->GetUTCharacter();
		}
	}

	Super::PostRender();

	UTPlayerOwner = NULL;
	UTCharacterOwner = NULL;

}

void AUTHUD::DrawHUD()
{
	Super::DrawHUD();

	// find center of the Canvas
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	for (int WidgetIndex=0; WidgetIndex < HudWidgets.Num(); WidgetIndex++)
	{
		// If we aren't hidden then set the canvas and render..
		if (HudWidgets[WidgetIndex] && !HudWidgets[WidgetIndex]->IsHidden())
		{
			HudWidgets[WidgetIndex]->PreDraw(this, Canvas, Center);
			if (!HudWidgets[WidgetIndex]->eventDraw(RenderDelta))
			{
				HudWidgets[WidgetIndex]->Draw(RenderDelta);
				HudWidgets[WidgetIndex]->PostDraw(GetWorld()->GetTimeSeconds());
			}
		}
	}


	const FVector2D CrossHairCenter(Canvas->ClipX * CrossHairCenterPoint.X, Canvas->ClipY * CrossHairCenterPoint.Y);

	// Draw very simple crosshair

	// offset by half the texture's dimensions so that the center of the texture aligns with the center of the Canvas
	const FVector2D CrosshairDrawPosition( (CrossHairCenter.X - (CrosshairTex->GetSurfaceWidth() * 0.5f)),
										   (CrossHairCenter.Y - (CrosshairTex->GetSurfaceHeight() * 0.5f)) );

	// draw the crosshair
	FCanvasTileItem TileItem( CrosshairDrawPosition, CrosshairTex->Resource, FLinearColor::White);
	TileItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem( TileItem );

	/**
	 * This is all TEMP code.  It will be replaced with a new hud system shortly but I 
	 * needed a way to display some data.  
	 **/

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		TempDrawString(FText::FromString(TEXT("!! Alpha Prototype !!")), Center.X, 5.0f, ETextHorzPos::Center, GEngine->GetSmallFont(), FLinearColor::White);
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

void AUTHUD::TempDrawNumber(int Number, float X, float Y, FLinearColor Color, float GlowOpacity, float Scale, int MinDigits, bool bRightAlign)
{
	const float FontPositions[10] = {633,297,325,365,403,441,480,519,556,594};
	const float FontSizes[10] = {40,28,40,38,38,39,39,37,38,39};

	// Convert the number to an ANSICHAR* so we can itterate 
	FString NumStr = FString::Printf(TEXT("%i"), Number);
	ANSICHAR *Ansi = TCHAR_TO_ANSI(*NumStr);

	if (bRightAlign)
	{
		Ansi += (NumStr.Len() -1);
	}

	int Cnt = NumStr.Len();
	if (Cnt < MinDigits)
	{
		Cnt = MinDigits;
	}

	for (int i=0;i<Cnt;i++)
	{
		int32 Index = 0;
		if (Cnt - i <= NumStr.Len())
		{
			Index = uint8(*Ansi) - 48;
			if (bRightAlign)
			{
				Ansi--;
			}
			else
			{
				Ansi++;
			}
		}

	
		if (Index >= 0 && Index <=9)
		{
			float U = FontPositions[Index];
			float Width = FontSizes[Index];
			float UL = Width;
			// Draw the background.
			Canvas->DrawColor = Color;
			Canvas->DrawColor.A *= GlowOpacity;
			Canvas->DrawTile(OldHudTexture, X, Y, 0.0, Width * Scale, 47 * Scale, U, 49, UL, 47, EBlendMode::BLEND_Translucent);
			Canvas->DrawColor = Color;
			Canvas->DrawTile(OldHudTexture, X, Y, 0.0, Width * Scale, 47 * Scale, U, 0, UL, 47, EBlendMode::BLEND_Translucent);

			X += (FontSizes[Index] * Scale * (bRightAlign? -1.0 : 1.0f));
		}

	}
}