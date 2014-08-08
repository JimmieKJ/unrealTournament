// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTHUDWidgetMessage.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTHUDWidgetMessage_DeathMessages.h"
#include "UTHUDWidgetMessage_ConsoleMessages.h"
#include "UTHUDWidget_WeaponInfo.h"
#include "UTHUDWidget_DMPlayerScore.h"
#include "UTHUDWidget_WeaponCrosshair.h"
#include "UTHUDWidget_Spectator.h"
#include "UTHUDWidget_WeaponBar.h"
#include "UTScoreboard.h"
#include "UTHUDWidget_Powerups.h"


AUTHUD::AUTHUD(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	WidgetOpacity = 1.0f;

	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("Texture2D'/Game/RestrictedAssets/Textures/crosshair.crosshair'"));
	DefaultCrosshairTex = CrosshairTexObj.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> OldHudTextureObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	OldHudTexture = OldHudTextureObj.Object;

	static ConstructorHelpers::FObjectFinder<UFont> SFont(TEXT("Font'/Game/RestrictedAssets/Fonts/fntSmallFont.fntSmallFont'"));
	SmallFont = SFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> MFont(TEXT("Font'/Game/RestrictedAssets/Fonts/fntMediumFont.fntMediumFont'"));
	MediumFont = MFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> LFont(TEXT("Font'/Game/RestrictedAssets/Fonts/fntLargeFont.fntLargeFont'"));
	LargeFont = LFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> EFont(TEXT("Font'/Game/RestrictedAssets/Fonts/fntExtreme.fntExtreme'"));
	ExtremeFont = LFont.Object;


	static ConstructorHelpers::FObjectFinder<UFont> NFont(TEXT("Font'/Game/RestrictedAssets/Fonts/fntNumbers.fntNumbers'"));
	NumberFont = NFont.Object;


	static ConstructorHelpers::FObjectFinder<UTexture2D> OldDamageIndicatorObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_DamageDir.UI_HUD_DamageDir'"));
	DamageIndicatorTexture = OldDamageIndicatorObj.Object;

	LastConfirmedHitTime = -100.0f;
}

void AUTHUD::BeginPlay()
{
	Super::BeginPlay();

	for (int i = 0; i < RequiredHudWidgetClasses.Num(); i++)
	{
		HudWidgetClasses.Add(ResolveHudWidgetByName(*RequiredHudWidgetClasses[i]));
	}

	for (int WidgetIndex = 0 ; WidgetIndex < HudWidgetClasses.Num(); WidgetIndex++)
	{
		AddHudWidget(HudWidgetClasses[WidgetIndex]);
	}


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

UFont* AUTHUD::GetFontFromSizeIndex(int32 FontSizeIndex) const
{
	switch (FontSizeIndex)
	{
		case 0: return GEngine->GetTinyFont();
		case 1: return SmallFont;
		case 2: return MediumFont;
		case 3: return LargeFont;
		case 4: return ExtremeFont;
	}

	return MediumFont;
}


TSubclassOf<UUTHUDWidget> AUTHUD::ResolveHudWidgetByName(const TCHAR* ResourceName)
{
	UClass* WidgetClass = LoadClass<UUTHUDWidget>(NULL, ResourceName, NULL, LOAD_None, NULL);

	if (WidgetClass != NULL)
	{
		return WidgetClass;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(NULL, ResourceName, NULL, LOAD_None, NULL);
	if (Blueprint != NULL)
	{
		WidgetClass = Blueprint->GeneratedClass;
		if (WidgetClass != NULL)
		{
			return WidgetClass;
		}
	}

	return NULL;
}



void AUTHUD::AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass)
{

	if (NewWidgetClass == NULL) return;

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

UUTHUDWidget* AUTHUD::FindHudWidgetByClass(TSubclassOf<UUTHUDWidget> SearchWidgetClass)
{
	for (int i=0;i<HudWidgets.Num();i++)
	{
		if (HudWidgets[i]->GetClass() == SearchWidgetClass)
		{
			return HudWidgets[i];
		}
	}
	return NULL;
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
				DrawString(FText::FromString(Text), 0, Y, ETextHorzPos::Left, GetFontFromSizeIndex(0), FLinearColor::White);
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

	// find center of the Canvas
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	AUTCharacter* UTCharacterOwner = NULL;
	if (UTPlayerOwner != NULL)
	{
		UTCharacterOwner = UTPlayerOwner->GetUTCharacter();
	}

	for (int WidgetIndex=0; WidgetIndex < HudWidgets.Num(); WidgetIndex++)
	{
		// If we aren't hidden then set the canvas and render..
		if (HudWidgets[WidgetIndex] && !HudWidgets[WidgetIndex]->IsHidden())
		{
			HudWidgets[WidgetIndex]->PreDraw(RenderDelta, this, Canvas, Center);
			if (HudWidgets[WidgetIndex]->ShouldDraw(bShowScores))
			{
				HudWidgets[WidgetIndex]->Draw(RenderDelta);
			}
			HudWidgets[WidgetIndex]->PostDraw(GetWorld()->GetTimeSeconds());
		}
	}


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
		DrawDamageIndicators();
	}
}

FText AUTHUD::ConvertTime(FText Prefix, FText Suffix, int Seconds) const
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
	Args.Add(TEXT("Prefix"), Prefix);
	Args.Add(TEXT("Suffix"), Suffix);

	return FText::Format( NSLOCTEXT("UTHUD","TIMERHOURS", "{Prefix}{Hours}:{Minutes}:{Seconds}{Suffix}"),Args);
}


void AUTHUD::DrawString(FText Text, float X, float Y, ETextHorzPos::Type HorzAlignment, ETextVertPos::Type VertAlignment, UFont* Font, FLinearColor Color, float Scale, bool bOutline)
{

	FVector2D RenderPos = FVector2D(X,Y);

	float XL, YL;
	Canvas->TextSize(Font, Text.ToString(), XL, YL, Scale, Scale);

	if (HorzAlignment != ETextHorzPos::Left)
	{
		RenderPos.X -= HorzAlignment == ETextHorzPos::Right ? XL : XL * 0.5f;
	}

	if (VertAlignment != ETextVertPos::Top)
	{
		RenderPos.Y -= VertAlignment == ETextVertPos::Bottom ? YL : YL * 0.5f;
	}

	FCanvasTextItem TextItem(RenderPos, Text, Font, Color);

	if (bOutline)
	{
		TextItem.bOutlined = true;
		TextItem.OutlineColor = FLinearColor::Black;
	}

	TextItem.Scale = FVector2D(Scale,Scale);
	Canvas->DrawItem(TextItem);
}

void AUTHUD::DrawNumber(int Number, float X, float Y, FLinearColor Color, float GlowOpacity, float Scale, int MinDigits, bool bRightAlign)
{
	FNumberFormattingOptions Opts;
	Opts.MinimumIntegralDigits = MinDigits;
	DrawString(FText::AsNumber(Number,&Opts), X, Y, bRightAlign ?  ETextHorzPos::Right : ETextHorzPos::Left, ETextVertPos::Top, NumberFont, Color, Scale, true);
}

void AUTHUD::PawnDamaged(FVector HitLocation, int32 DamageAmount, TSubclassOf<UDamageType> DamageClass, bool bFriendlyFire)
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
		DamageIndicators[BestIndex].bFriendlyFire = bFriendlyFire;

//		UUTHUDWidget_WeaponCrosshair* CrossHairWidget =
	

	}
}

void AUTHUD::DrawDamageIndicators()
{
	for (int i=0; i < DamageIndicators.Num(); i++)
	{
		if (DamageIndicators[i].FadeTime > 0.0f)
		{
			FLinearColor DrawColor = DamageIndicators[i].bFriendlyFire ? FLinearColor::Green : FLinearColor::Red;
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

void AUTHUD::CausedDamage(APawn* HitPawn, int32 Damage)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL || !GS->OnSameTeam(HitPawn, PlayerOwner))
	{
		LastConfirmedHitTime = GetWorld()->TimeSeconds;
	}
	else
	{
		// TODO: team damage - draw "don't do that!" indicator? but need to make sure enemy hitconfirms have priority
	}
}

FLinearColor AUTHUD::GetBaseHUDColor()
{
	return FLinearColor::White;
}

