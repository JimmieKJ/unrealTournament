// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTHUDWidgetMessage.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTHUDWidgetMessage_DeathMessages.h"
#include "UTHUDWidgetMessage_ConsoleMessages.h"
#include "UTHUDWidget_DMPlayerScore.h"
#include "UTHUDWidget_DMPlayerLeaderboard.h"
#include "UTHUDWidget_WeaponInfo.h"
#include "UTScoreboard.h"



AUTHUD::AUTHUD(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	WidgetOpacity = 1.0f;

	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("Texture2D'/Game/RestrictedAssets/Textures/crosshair.crosshair'"));
	DefaultCrosshairTex = CrosshairTexObj.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> OldHudTextureObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	OldHudTexture = OldHudTextureObj.Object;

	static ConstructorHelpers::FObjectFinder<UFont> MFont(TEXT("Font'/Game/RestrictedAssets/Proto/UI/Fonts/fntRobotoBlack36.fntRobotoBlack36'"));
	MediumFont = MFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> LFont(TEXT("Font'/Game/RestrictedAssets/Proto/UI/Fonts/fntRobotoBlack72.fntRobotoBlack72'"));
	LargeFont = LFont.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> OldDamageIndicatorObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_DamageDir.UI_HUD_DamageDir'"));
	DamageIndicatorTexture = OldDamageIndicatorObj.Object;

}

void AUTHUD::BeginPlay()
{
	Super::BeginPlay();

	for (int WidgetIndex = 0 ; WidgetIndex < HudWidgetClasses.Num(); WidgetIndex++)
	{
		AddHudWidget(HudWidgetClasses[WidgetIndex]);
	}

	AddHudWidget(UUTHUDWidget_Paperdoll::StaticClass());
	AddHudWidget(UUTHUDWidget_WeaponInfo::StaticClass());
	AddHudWidget(UUTHUDWidget_DMPlayerScore::StaticClass());
	AddHudWidget(UUTHUDWidget_DMPlayerLeaderboard::StaticClass());
	AddHudWidget(UUTHUDWidgetMessage_DeathMessages::StaticClass());
	AddHudWidget(UUTHUDWidgetMessage_ConsoleMessages::StaticClass());

	AddHudWidget(TEXT("Blueprint'/Game/RestrictedAssets/Blueprints/GameMessageWidget.GameMessageWidget'"));

	DamageIndicators.AddZeroed(MAX_DAMAGE_INDICATORS);
	for (int i=0;i<MAX_DAMAGE_INDICATORS;i++)
	{
		DamageIndicators[i].RotationAngle = 0.0f;
		DamageIndicators[i].DamageAmount = 0.0f;
		DamageIndicators[i].FadeTime = 0.0f;
	}
}

void AUTHUD::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	UTPlayerOwner = Cast<AUTPlayerController>(GetOwner());
}

void AUTHUD::AddHudWidget(const TCHAR* NewWidgetClassName)
{
	UClass* WidgetClass = LoadClass<UUTHUDWidget>(NULL, NewWidgetClassName, NULL, LOAD_None, NULL);

	if (WidgetClass != NULL)
	{
		AddHudWidget(WidgetClass);
		return;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(NULL, NewWidgetClassName, NULL, LOAD_None, NULL);
	if (Blueprint != NULL)
	{
		WidgetClass = Blueprint->GeneratedClass;
		if (WidgetClass != NULL)
		{
			AddHudWidget(WidgetClass);
			return;
		}
	}
}



void AUTHUD::AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass)
{
	UUTHUDWidget* Widget = ConstructObject<UUTHUDWidget>(NewWidgetClass,GetTransientPackage());
	HudWidgets.Add(Widget);

	// If this widget is a messaging widget, then track it
	UUTHUDWidgetMessage* MessageWidget = Cast<UUTHUDWidgetMessage>(Widget);
	if (MessageWidget != NULL)
	{
		HudMessageWidgets.Add(MessageWidget->ManagedMessageArea, MessageWidget);
	}

	Widget->InitializeWidget(this);
}

void AUTHUD::CreateScoreboard(TSubclassOf<class UUTScoreboard> NewScoreboardClass)
{
	MyUTScoreboard = ConstructObject<UUTScoreboard>(NewScoreboardClass, GetTransientPackage());
}

void AUTHUD::ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject)
{
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

void AUTHUD::ToggleScoreboard(bool bShow)
{
	bShowScores = bShow;
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
	Super::PostRender();
}

void AUTHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bShowScores)
	{
		if (MyUTScoreboard != NULL)
		{

			MyUTScoreboard->Canvas = Canvas;
			MyUTScoreboard->UTHUDOwner = this;
			MyUTScoreboard->UTGameState = GetWorld()->GetGameState<AUTGameState>();

			if (Canvas && MyUTScoreboard->UTGameState)
			{
				MyUTScoreboard->DrawScoreboard(RenderDelta);
			}

			MyUTScoreboard->Canvas = NULL;
			MyUTScoreboard->UTHUDOwner = NULL;
			MyUTScoreboard->UTGameState = NULL;
		}
	}
	else
	{
		// find center of the Canvas
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

		// Let the weapon draw if needed...
		if (UTPlayerOwner != NULL && UTPlayerOwner->GetUTCharacter() != NULL && UTPlayerOwner->GetUTCharacter()->GetWeapon() != NULL)
		{
			UTPlayerOwner->GetUTCharacter()->GetWeapon()->DrawHud(this, Canvas);
		}

		for (int WidgetIndex=0; WidgetIndex < HudWidgets.Num(); WidgetIndex++)
		{
			// If we aren't hidden then set the canvas and render..
			if (HudWidgets[WidgetIndex] && !HudWidgets[WidgetIndex]->IsHidden())
			{
				HudWidgets[WidgetIndex]->PreDraw(RenderDelta, this, Canvas, Center);
				if (!HudWidgets[WidgetIndex]->eventDraw(RenderDelta))
				{
					HudWidgets[WidgetIndex]->Draw(RenderDelta);
					HudWidgets[WidgetIndex]->PostDraw(GetWorld()->GetTimeSeconds());
				}
			}
		}

		DrawDamageIndicators();

		/**
		 * This is all TEMP code.  It will be replaced with a new hud system shortly but I 
		 * needed a way to display some data.  
		 **/

		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		if (GameState)
		{
			TempDrawString(FText::FromString(TEXT("!! Alpha Prototype !!")), Center.X, 5.0f, ETextHorzPos::Center, ETextVertPos::Top, GEngine->GetSmallFont(), FLinearColor::White);
			TempDrawString( TempConvertTime(GameState->ElapsedTime), Center.X, 20, ETextHorzPos::Center, ETextVertPos::Top, GEngine->GetMediumFont(), FLinearColor::White);
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


void AUTHUD::TempDrawString(FText Text, float X, float Y, ETextHorzPos::Type HorzAlignment, ETextVertPos::Type VertAlignment, UFont* Font, FLinearColor Color, float Scale)
{

	FVector2D RenderPos = FVector2D(X,Y);

	int32 XL, YL;
	Font->GetStringHeightAndWidth(Text.ToString(), YL, XL);

	if (HorzAlignment != ETextHorzPos::Left)
	{
		RenderPos.X -= HorzAlignment == ETextHorzPos::Right ? XL : XL * 0.5f;
	}

	if (VertAlignment != ETextVertPos::Top)
	{
		RenderPos.Y -= VertAlignment == ETextVertPos::Bottom ? YL : YL * 0.5f;
	}

	FCanvasTextItem TextItem(RenderPos, Text, Font, Color);
	TextItem.Scale = FVector2D(Scale,Scale);
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

			if (bRightAlign)
			{
				X -= FontSizes[Index] * Scale;
			}

			float U = FontPositions[Index];
			float Width = FontSizes[Index];
			float UL = Width;
			// Draw the background.
			Canvas->DrawColor = Color;
			Canvas->DrawColor.A *= GlowOpacity;
			Canvas->DrawTile(OldHudTexture, X, Y, Width * Scale, 47 * Scale, U, 49, UL, 47, EBlendMode::BLEND_Translucent);
			Canvas->DrawColor = Color;
			Canvas->DrawTile(OldHudTexture, X, Y, Width * Scale, 47 * Scale, U, 0, UL, 47, EBlendMode::BLEND_Translucent);

			if (!bRightAlign)
			{
				X += FontSizes[Index] * Scale;
			}
		}

	}
}

void AUTHUD::PawnDamaged(FVector HitLocation, float DamageAmount, TSubclassOf<UDamageType> DamageClass)
{
	// Calculate the rotation 	
	AUTCharacter* UTC = UTPlayerOwner->GetUTCharacter();
	if (UTC != NULL && ! UTC->IsDead())	// If have a pawn and it's alive...
	{
		FVector CharacterLocation;
		FRotator CharacterRotation;

		UTC->GetActorEyesViewPoint(CharacterLocation, CharacterRotation);
		FVector HitSafeNormal = (HitLocation - CharacterLocation).SafeNormal2D();
		float Ang = FMath::Acos(FVector::DotProduct(CharacterRotation.Vector().SafeNormal2D(), HitSafeNormal)) * (180.0f / PI);

		// Figure out Left/Right....
		float FinalAng = ( FVector::DotProduct( FVector::CrossProduct(CharacterRotation.Vector(), FVector(0,0,1)), HitSafeNormal)) > 0 ? 360 - Ang : Ang;

		int BestIndex = 0;
		float BestTime = DamageIndicators[0].FadeTime;
		for (int i=0; i < MAX_DAMAGE_INDICATORS; i++)
		{
			if (DamageIndicators[i].FadeTime <= 0.0f)					
			{
				BestIndex = i;
				break;
			}
			else
			{
				if (DamageIndicators[i].FadeTime < BestTime)
				{
					BestIndex = i;
					BestTime = DamageIndicators[i].FadeTime;
				}
			}
		}

		DamageIndicators[BestIndex].FadeTime = DAMAGE_FADE_DURATION;
		DamageIndicators[BestIndex].RotationAngle = FinalAng;
	}
}

void AUTHUD::DrawDamageIndicators()
{
	FLinearColor DrawColor = FLinearColor::White;
	for (int i=0; i < DamageIndicators.Num(); i++)
	{
		if (DamageIndicators[i].FadeTime > 0.0f)
		{
			DrawColor.A = 1.0 * (DamageIndicators[i].FadeTime / DAMAGE_FADE_DURATION);

			float Size = 384 * (Canvas->ClipY / 720.0f);
			float Half = Size * 0.5;

			FCanvasTileItem ImageItem(FVector2D((Canvas->ClipX * 0.5) - Half, (Canvas->ClipY * 0.5) - Half), DamageIndicatorTexture->Resource, FVector2D(Size, Size), FVector2D(0,0), FVector2D(1,1), DrawColor);
			ImageItem.Rotation = FRotator(0,DamageIndicators[i].RotationAngle,0);
			ImageItem.PivotPoint = FVector2D(0.5,0.5);
			ImageItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_Translucent;
			Canvas->DrawItem( ImageItem );

			DamageIndicators[i].FadeTime -= RenderDelta;
		}
	}
}

