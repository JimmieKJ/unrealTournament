// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTHUDWidgetMessage.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTHUDWidgetMessage_DeathMessages.h"


AUTHUD::AUTHUD(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("/Game/Textures/Crosshair"));
	CrosshairTex = CrosshairTexObj.Object;
}

void AUTHUD::BeginPlay()
{
	Super::BeginPlay();

	for (int WidgetIndex = 0 ; WidgetIndex < HudWidgetClasses.Num(); WidgetIndex++)
	{
		AddHudWidget(HudWidgetClasses[WidgetIndex]);
	}

	AddHudWidget(UUTHUDWidget_Paperdoll::StaticClass());
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

	// Draw very simple crosshair

	// offset by half the texture's dimensions so that the center of the texture aligns with the center of the Canvas
	const FVector2D CrosshairDrawPosition( (Center.X - (CrosshairTex->GetSurfaceWidth() * 0.5f)),
										   (Center.Y - (CrosshairTex->GetSurfaceHeight() * 0.5f)) );

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

		AUTPlayerState* OwnerState = Cast<AUTPlayerState>(PlayerOwner->PlayerState);
		if (OwnerState)
		{
			TempDrawString( FText::Format(NSLOCTEXT("UTHUD","Score","{0} : Score"), FText::AsNumber(OwnerState->Score)), Canvas->ClipX - 5.0f, 5.0f, ETextHorzPos::Right, GEngine->GetLargeFont(), FLinearColor::White);
			TempDrawString( FText::Format(NSLOCTEXT("UTHUD","Deaths","{0} : Deaths"), FText::AsNumber(OwnerState->Deaths)), Canvas->ClipX - 5.0f, 25.0f, ETextHorzPos::Right, GEngine->GetSmallFont(), FLinearColor::White);
		}
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