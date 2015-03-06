// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
#include "Json.h"
#include "DisplayDebugHelpers.h"


AUTHUD::AUTHUD(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
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
	LastPickupTime = -100.f;

	bShowOverlays = true;
}

void AUTHUD::BeginPlay()
{
	Super::BeginPlay();

	// Parse the widgets found in the ini
	for (int i = 0; i < RequiredHudWidgetClasses.Num(); i++)
	{
		BuildHudWidget(*RequiredHudWidgetClasses[i]);
	}

	// Parse any hard coded widgets
	for (int WidgetIndex = 0 ; WidgetIndex < HudWidgetClasses.Num(); WidgetIndex++)
	{
		BuildHudWidget(HudWidgetClasses[WidgetIndex]);
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


void AUTHUD::ShowDebugInfo(float& YL, float& YPos)
{
	if (!DebugDisplay.Contains(TEXT("Bones")))
	{
		FLinearColor BackgroundColor(0.f, 0.f, 0.f, 0.2f);
		DebugCanvas->Canvas->DrawTile(0, 0, 0.5f*DebugCanvas->ClipX, 0.5f*DebugCanvas->ClipY, 0.f, 0.f, 0.f, 0.f, BackgroundColor);
	}

	FDebugDisplayInfo DisplayInfo(DebugDisplay, ToggledDebugCategories);
	PlayerOwner->PlayerCameraManager->ViewTarget.Target->DisplayDebug(DebugCanvas, DisplayInfo, YL, YPos);

	if (ShouldDisplayDebug(NAME_Game))
	{
		GetWorld()->GetAuthGameMode()->DisplayDebug(DebugCanvas, DisplayInfo, YL, YPos);
	}
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

AUTPlayerState* AUTHUD::GetViewedPlayerState()
{
	AUTPlayerState* PS = UTPlayerOwner->UTPlayerState;
	APawn* PawnOwner = (UTPlayerOwner->GetPawn() != NULL) ? UTPlayerOwner->GetPawn() : Cast<APawn>(UTPlayerOwner->GetViewTarget());
	if (PawnOwner != NULL && Cast<AUTPlayerState>(PawnOwner->PlayerState) != NULL)
	{
		PS = (AUTPlayerState*)PawnOwner->PlayerState;
	}
	return PS;
}

TSubclassOf<UUTHUDWidget> AUTHUD::ResolveHudWidgetByName(const TCHAR* ResourceName)
{
	UClass* WidgetClass = LoadClass<UUTHUDWidget>(NULL, ResourceName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);

	if (WidgetClass != NULL)
	{
		return WidgetClass;
	}
	FString BlueprintResourceName = FString::Printf(TEXT("%s_C"), ResourceName);
	
	WidgetClass = LoadClass<UUTHUDWidget>(NULL, *BlueprintResourceName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (WidgetClass != NULL)
	{
		return WidgetClass;
	}

	return NULL;
}

FVector2D AUTHUD::JSon2FVector2D(const TSharedPtr<FJsonObject> Vector2DObject, FVector2D Default)
{
	FVector2D Final = Default;

	// Grab X
	const TSharedPtr<FJsonValue>* XVal = Vector2DObject->Values.Find(TEXT("X"));
	if (XVal != NULL && (*XVal)->Type == EJson::Number) Final.X = (*XVal)->AsNumber();

	// Grab Y
	const TSharedPtr<FJsonValue>* YVal = Vector2DObject->Values.Find(TEXT("Y"));
	if (YVal != NULL && (*YVal)->Type == EJson::Number) Final.Y = (*YVal)->AsNumber();

	return Final;
}


void AUTHUD::BuildHudWidget(FString NewWidgetString)
{
	// Look at the string.  If it starts with a "{" then assume it's not a JSON based config and just resolve it's name.

	if ( NewWidgetString.Trim().Left(1) == TEXT("{") )
	{
		// It's a json command so we have to break it apart

		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( NewWidgetString );
		TSharedPtr<FJsonObject> JSONObject;
		if (FJsonSerializer::Deserialize( Reader, JSONObject) && JSONObject.IsValid() )
		{
			// We have a valid JSON object..

			const TSharedPtr<FJsonValue>* ClassName = JSONObject->Values.Find(TEXT("Classname"));
			if (ClassName->IsValid() && (*ClassName)->Type == EJson::String)
			{
				TSubclassOf<UUTHUDWidget> NewWidgetClass = ResolveHudWidgetByName(*(*ClassName)->AsString());
				if (NewWidgetClass != NULL) 
				{
					UUTHUDWidget* NewWidget = AddHudWidget(NewWidgetClass);

					// Now Look for position Overrides

					const TSharedPtr<FJsonValue>* PositionVal = JSONObject->Values.Find(TEXT("Position"));
					if (PositionVal != NULL && (*PositionVal)->Type == EJson::Object) 
					{
						NewWidget->Position = JSon2FVector2D( (*PositionVal)->AsObject(), NewWidget->Position);
					}
				
					const TSharedPtr<FJsonValue>* OriginVal = JSONObject->Values.Find(TEXT("Origin"));
					if (OriginVal != NULL && (*OriginVal)->Type == EJson::Object) 
					{
						NewWidget->Origin = JSon2FVector2D( (*OriginVal)->AsObject(), NewWidget->Origin);
					}

					const TSharedPtr<FJsonValue>* ScreenPositionVal = JSONObject->Values.Find(TEXT("ScreenPosition"));
					if (ScreenPositionVal != NULL && (*ScreenPositionVal)->Type == EJson::Object) 
					{
						NewWidget->ScreenPosition = JSon2FVector2D( (*ScreenPositionVal)->AsObject(), NewWidget->ScreenPosition);
					}

					const TSharedPtr<FJsonValue>* SizeVal = JSONObject->Values.Find(TEXT("Size"));
					if (SizeVal != NULL && (*SizeVal)->Type == EJson::Object)
					{
						NewWidget->Size = JSon2FVector2D( (*SizeVal)->AsObject(), NewWidget->Size);
					}


				}
			}
		}
		else
		{
			UE_LOG(UT,Log,TEXT("Failed to parse JSON HudWidget entry: %s"),*NewWidgetString);
		}
	}
	else
	{
		TSubclassOf<UUTHUDWidget> NewWidgetClass = ResolveHudWidgetByName(*NewWidgetString);
		if (NewWidgetClass != NULL) AddHudWidget(NewWidgetClass);
	}
}




UUTHUDWidget* AUTHUD::AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass)
{

	if (NewWidgetClass == NULL) return NULL;

	UUTHUDWidget* Widget = ConstructObject<UUTHUDWidget>(NewWidgetClass,GetTransientPackage());
	HudWidgets.Add(Widget);

	// If this widget is a messaging widget, then track it
	UUTHUDWidgetMessage* MessageWidget = Cast<UUTHUDWidgetMessage>(Widget);
	if (MessageWidget != NULL)
	{
		HudMessageWidgets.Add(MessageWidget->ManagedMessageArea, MessageWidget);
	}
	// cache ref to scoreboard (NOTE: only one!)
	if (Widget->IsA(UUTScoreboard::StaticClass()))
	{
		MyUTScoreboard = Cast<UUTScoreboard>(Widget);
	}

	Widget->InitializeWidget(this);
	return Widget;
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
	//MyUTScoreboard = ConstructObject<UUTScoreboard>(NewScoreboardClass, GetTransientPackage());
	// Scoreboards are now HUD widgets
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
	ScoreboardPage = 0; // TODO: not sure if we should remember or reset this
}

void AUTHUD::PostRender()
{
	// Always sort the PlayerState array at the beginning of each frame
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		GS->SortPRIArray();
	}

	CalcStanding();

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

	if (!IsPendingKillPending() || !IsPendingKill())
	{
		// find center of the Canvas
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		bool bPreMatchScoreBoard = (GS && !GS->HasMatchStarted() && !GS->IsMatchInCountdown());

		for (int WidgetIndex = 0; WidgetIndex < HudWidgets.Num(); WidgetIndex++)
		{
			// If we aren't hidden then set the canvas and render..
			if (HudWidgets[WidgetIndex] && !HudWidgets[WidgetIndex]->IsHidden() && !HudWidgets[WidgetIndex]->IsPendingKill())
			{
				HudWidgets[WidgetIndex]->PreDraw(RenderDelta, this, Canvas, Center);
				if (HudWidgets[WidgetIndex]->ShouldDraw(bShowScores || bPreMatchScoreBoard || bForceScores))
				{
					HudWidgets[WidgetIndex]->Draw(RenderDelta);
				}
				HudWidgets[WidgetIndex]->PostDraw(GetWorld()->GetTimeSeconds());
			}
		}

		if (!bShowScores)
		{
			DrawDamageIndicators();
		}
	}

}

FText AUTHUD::ConvertTime(FText Prefix, FText Suffix, int Seconds, bool bForceHours, bool bForceMinutes, bool bForceTwoDigits) const
{
	int Hours = Seconds / 3600;
	Seconds -= Hours * 3600;
	int Mins = Seconds / 60;
	Seconds -= Mins * 60;
	bool bDisplayHours = bForceHours || Hours > 0;
	bool bDisplayMinutes = bDisplayHours || bForceMinutes || Mins > 0;

	FFormatNamedArguments Args;
	FNumberFormattingOptions Options;

	Options.MinimumIntegralDigits = 2;
	Options.MaximumIntegralDigits = 2;

	Args.Add(TEXT("Hours"), FText::AsNumber(Hours, NULL));
	Args.Add(TEXT("Minutes"), FText::AsNumber(Mins, (bDisplayHours || bForceTwoDigits) ? &Options : NULL));
	Args.Add(TEXT("Seconds"), FText::AsNumber(Seconds, (bDisplayMinutes || bForceTwoDigits) ? &Options : NULL));
	Args.Add(TEXT("Prefix"), Prefix);
	Args.Add(TEXT("Suffix"), Suffix);

	if (bDisplayHours)
	{
		return FText::Format(NSLOCTEXT("UTHUD", "TIMERHOURS", "{Prefix}{Hours}:{Minutes}:{Seconds}{Suffix}"), Args);
	}
	else if (bDisplayMinutes)
	{
		return FText::Format(NSLOCTEXT("UTHUD", "TIMERHOURS", "{Prefix}{Minutes}:{Seconds}{Suffix}"), Args);
	}
	else
	{
		return FText::Format(NSLOCTEXT("UTHUD", "TIMERHOURS", "{Prefix}{Seconds}{Suffix}"), Args);
	}
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
	if (UTC != NULL && ! UTC->IsDead() && (DamageAmount > 0))	// If have a pawn and it's alive...
	{
		FVector CharacterLocation;
		FRotator CharacterRotation;

		UTC->GetActorEyesViewPoint(CharacterLocation, CharacterRotation);
		FVector HitSafeNormal = (HitLocation - CharacterLocation).GetSafeNormal2D();
		float Ang = FMath::Acos(FVector::DotProduct(CharacterRotation.Vector().GetSafeNormal2D(), HitSafeNormal)) * (180.0f / PI);

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
		if (DamageAmount > 0)
		{
			UTC->PlayDamageEffects();
		}
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

FLinearColor AUTHUD::GetWidgetTeamColor()
{
	// Add code to cache and return the team color if it's a team game

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL || (GS->bTeamGame && UTPlayerOwner && UTPlayerOwner->UTPlayerState && UTPlayerOwner->UTPlayerState->Team))
	{
		//return UTPlayerOwner->UTPlayerState->Team->TeamColor;
		return (UTPlayerOwner->GetTeamNum() == 0) ? FLinearColor(0.15,0.0,0.0,1.0) : FLinearColor(0.025,0.025,0.1,1.0);
	}

	return FLinearColor::Black;
}

void AUTHUD::CalcStanding()
{
	// NOTE: By here in the Hud rendering chain, the PlayerArray in the GameState
	// has been sorted.

	Leaderboard.Empty();

	CurrentPlayerStanding = 0;
	CurrentPlayerSpread = 0;
	CurrentPlayerScore = 0;
	NumActualPlayers = 0;

	AUTPlayerState* MyPS = GetViewedPlayerState();

	if (!UTPlayerOwner || !MyPS) return;	// Quick out if not ready

	CurrentPlayerScore = int32(MyPS->Score);

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{

		// Build the leaderboard.
		for (int i=0;i<GameState->PlayerArray.Num();i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
			if (PS != NULL && !PS->bIsSpectator)
			{

				// Sort in to the leaderboard

				int32 Index = -1;
				for (int32 j=0;j<Leaderboard.Num();j++)
				{
					if (PS->Score > Leaderboard[j]->Score)
					{
						Index = j;
						break;
					}
				}

				if (Index >=0)
				{
					Leaderboard.Insert(PS, Index);
				}
				else
				{
					Leaderboard.Add(PS);
				}
			}
		}
		
		NumActualPlayers = Leaderboard.Num();

		// Find my index in it.

		CurrentPlayerStanding = 1;
		int MyIndex = Leaderboard.Find(MyPS);
		if (MyIndex >= 0)
		{
			for (int32 i=0; i < MyIndex; i++)
			{
				if (Leaderboard[i]->Score > MyPS->Score)
				{
					CurrentPlayerStanding++;
				}
			}
		}

		if (CurrentPlayerStanding > 1)
		{
			CurrentPlayerSpread = MyPS->Score - Leaderboard[0]->Score;
		}
		else if (MyIndex < Leaderboard.Num()-1)
		{
			CurrentPlayerSpread = MyPS->Score - Leaderboard[MyIndex+1]->Score;
		}

		if ( Leaderboard.Num() > 0 && Leaderboard[0]->Score == MyPS->Score && Leaderboard[0] != MyPS)
		{
			// Bubble this player to the top
			Leaderboard.Remove(MyPS);
			Leaderboard.Insert(MyPS,0);
		}

	}
}

FText AUTHUD::GetPlaceSuffix(int32 Value)
{
	switch (Value)
	{
		case 0: return FText::GetEmpty(); break;
		case 1:  return NSLOCTEXT("UTHUD","FirstPlaceSuffix","st"); break;
		case 2:  return NSLOCTEXT("UTHUD","SecondPlaceSuffix","nd"); break;
		case 3:  return NSLOCTEXT("UTHUD","ThirdPlaceSuffix","rd"); break;
		default: return NSLOCTEXT("UTHUD","NthPlaceSuffix","th"); break;
	}

	return FText::GetEmpty();
}

