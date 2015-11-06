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
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTScoreboard.h"
#include "UTHUDWidget_Powerups.h"
#include "Json.h"
#include "DisplayDebugHelpers.h"
#include "UTRemoteRedeemer.h"
#include "UTGameEngine.h"
#include "UTFlagInfo.h"
#include "UTCrosshair.h"
#include "UTATypes.h"
#include "UTDemoRecSpectator.h"

AUTHUD::AUTHUD(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetOpacity = 1.0f;

	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("Texture2D'/Game/RestrictedAssets/Textures/crosshair.crosshair'"));
	DefaultCrosshairTex = CrosshairTexObj.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> OldHudTextureObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	OldHudTexture = OldHudTextureObj.Object;

	static ConstructorHelpers::FObjectFinder<UFont> TFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Tiny.fntScoreboard_Tiny'"));
	TinyFont = TFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> SFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Small.fntScoreboard_Small'"));
	SmallFont = SFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> MFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Medium.fntScoreboard_Medium'"));
	MediumFont = MFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> LFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Large.fntScoreboard_Large'"));
	LargeFont = LFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> HFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Huge.fntScoreboard_Huge'"));
	HugeFont = HFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> ScFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Score.fntScoreboard_Score'"));
	ScoreFont = ScFont.Object;

	// non-proportional FIXMESTEVE need better font and just numbers
	static ConstructorHelpers::FObjectFinder<UFont> CFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Clock.fntScoreboard_Clock'"));
	NumberFont = CFont.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> OldDamageIndicatorObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_DamageDir.UI_HUD_DamageDir'"));
	DamageIndicatorTexture = OldDamageIndicatorObj.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> HUDTex(TEXT("Texture'/Game/RestrictedAssets/UI/HUDAtlas01.HUDAtlas01'"));
	HUDAtlas = HUDTex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> PlayerDirectionTextureObject(TEXT("/Game/RestrictedAssets/UI/MiniMap/Minimap_PS_BG.Minimap_PS_BG"));
	PlayerMinimapTexture = PlayerDirectionTextureObject.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> SelectedPlayerTextureObject(TEXT("/Game/RestrictedAssets/Weapons/Sniper/Assets/TargetCircle.TargetCircle"));
	SelectedPlayerTexture = SelectedPlayerTextureObject.Object;

	LastConfirmedHitTime = -100.0f;
	LastPickupTime = -100.f;
	bFontsCached = false;
	bShowOverlays = true;
	bHaveAddedSpectatorWidgets = false;

	TeamIconUV[0] = FVector2D(257.f, 940.f);
	TeamIconUV[1] = FVector2D(333.f, 940.f);

	bDrawChatKillMsg = false;
	bDrawPopupKillMsg = true;

	bCustomWeaponCrosshairs = true;
}

void AUTHUD::BeginPlay()
{
	Super::BeginPlay();

	// Parse the widgets found in the ini
	for (int32 i = 0; i < RequiredHudWidgetClasses.Num(); i++)
	{
		BuildHudWidget(*RequiredHudWidgetClasses[i]);
	}

	// Parse any hard coded widgets
	for (int32 WidgetIndex = 0 ; WidgetIndex < HudWidgetClasses.Num(); WidgetIndex++)
	{
		BuildHudWidget(HudWidgetClasses[WidgetIndex]);
	}

	DamageIndicators.AddZeroed(MAX_DAMAGE_INDICATORS);
	for (int32 i=0;i<MAX_DAMAGE_INDICATORS;i++)
	{
		DamageIndicators[i].RotationAngle = 0.0f;
		DamageIndicators[i].DamageAmount = 0.0f;
		DamageIndicators[i].FadeTime = 0.0f;
	}
}

void AUTHUD::AddSpectatorWidgets()
{
	if (bHaveAddedSpectatorWidgets)
	{
		return;
	}
	bHaveAddedSpectatorWidgets = true;

	// Parse the widgets found in the ini
	for (int32 i = 0; i < SpectatorHudWidgetClasses.Num(); i++)
	{
		BuildHudWidget(*SpectatorHudWidgetClasses[i]);
	}

	UUTLocalPlayer* UTLP = UTPlayerOwner ? Cast<UUTLocalPlayer>(UTPlayerOwner->Player) : NULL;
	if (UTLP)
	{
		UTLP->OpenSpectatorWindow();
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
	case 0: return TinyFont;
	case 1: return SmallFont;
	case 2: return MediumFont;
	case 3: return LargeFont;
	}

	return MediumFont;
}

AUTPlayerState* AUTHUD::GetScorerPlayerState()
{
	AUTPlayerState* PS = UTPlayerOwner->UTPlayerState;
	if (PS && !PS->bOnlySpectator)
	{
		// view your own score unless you are a spectator
		return PS;
	}
	APawn* PawnOwner = (UTPlayerOwner->GetPawn() != NULL) ? UTPlayerOwner->GetPawn() : Cast<APawn>(UTPlayerOwner->GetViewTarget());
	if (PawnOwner != NULL && Cast<AUTPlayerState>(PawnOwner->PlayerState) != NULL)
	{
		PS = (AUTPlayerState*)PawnOwner->PlayerState;
	}

	return UTPlayerOwner->LastSpectatedPlayerState ? UTPlayerOwner->LastSpectatedPlayerState : PS;
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
	return WidgetClass;
}

FVector2D AUTHUD::JSon2FVector2D(const TSharedPtr<FJsonObject> Vector2DObject, FVector2D Default)
{
	FVector2D Final = Default;

	const TSharedPtr<FJsonValue>* XVal = Vector2DObject->Values.Find(TEXT("X"));
	if (XVal != NULL && (*XVal)->Type == EJson::Number) Final.X = (*XVal)->AsNumber();

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

bool AUTHUD::HasHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass)
{
	if ((NewWidgetClass == NULL) || (HudWidgets.Num() == 0))
	{
		return false;
	}

	for (int32 i = 0; i < HudWidgets.Num(); i++)
	{
		if (HudWidgets[i] && (HudWidgets[i]->GetClass() == NewWidgetClass))
		{
			return true;
		}
	}
	return false;
}

UUTHUDWidget* AUTHUD::AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass)
{
	if (NewWidgetClass == NULL) return NULL;

	UUTHUDWidget* Widget = NewObject<UUTHUDWidget>(GetTransientPackage(), NewWidgetClass);
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
	if (Cast<UUTHUDWidget_Spectator>(Widget))
	{
		SpectatorMessageWidget = Cast<UUTHUDWidget_Spectator>(Widget);
	}
	if (Cast<UUTHUDWidget_ReplayTimeSlider>(Widget))
	{
		ReplayTimeSliderWidget = Cast<UUTHUDWidget_ReplayTimeSlider>(Widget);
	}
	if (Cast<UUTHUDWidget_SpectatorSlideOut>(Widget))
	{
		SpectatorSlideOutWidget = Cast<UUTHUDWidget_SpectatorSlideOut>(Widget);
	}

	return Widget;
}

UUTHUDWidget* AUTHUD::FindHudWidgetByClass(TSubclassOf<UUTHUDWidget> SearchWidgetClass, bool bExactClass)
{
	for (int32 i=0; i<HudWidgets.Num(); i++)
	{
		if (bExactClass ? HudWidgets[i]->GetClass() == SearchWidgetClass : HudWidgets[i]->IsA(SearchWidgetClass))
		{
			return HudWidgets[i];
		}
	}
	return NULL;
}

void AUTHUD::CreateScoreboard(TSubclassOf<class UUTScoreboard> NewScoreboardClass)
{
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
		UE_LOG(UT,Verbose,TEXT("No Message Widget to Display Text"));
	}
}

void AUTHUD::ToggleScoreboard(bool bShow)
{
	if (!bShowScores)
	{
		ScoreboardPage = 0;
	}
	bShowScores = bShow;
}

void AUTHUD::NotifyMatchStateChange()
{
	// FIXMESTEVE - in playerintro mode, open match summary if not open (option for UTLP openmatchsummary)
	UUTLocalPlayer* UTLP = UTPlayerOwner ? Cast<UUTLocalPlayer>(UTPlayerOwner->Player) : NULL;
	AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GetGameState());
	if (UTLP && GS && !GS->IsPendingKillPending())
	{
		if (GS->GetMatchState() == MatchState::WaitingPostMatch)
		{
			if (GS->GameModeClass != nullptr)
			{
				AUTGameMode* UTGameMode = GS->GameModeClass->GetDefaultObject<AUTGameMode>();
				if (UTGameMode != nullptr && !UTGameMode->bShowMatchSummary)
				{
					return;
				}
			}

			AUTGameMode* DefaultGame = Cast<AUTGameMode>(GS->GetDefaultGameMode());
			float MatchSummaryDelay = DefaultGame ? DefaultGame->EndScoreboardDelay + DefaultGame->MainScoreboardDisplayTime + DefaultGame->ScoringPlaysDisplayTime : 10.f;
			GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD::OpenMatchSummary, MatchSummaryDelay, false);
		}
		else if (GS->GetMatchState() == MatchState::WaitingToStart)
		{
			// GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD::OpenMatchSummary, 0.5f, false);
		}
		else if (GS->GetMatchState() == MatchState::PlayerIntro)
		{
			GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD::OpenMatchSummary, 0.2f, false);
		}
		else
		{
			UTLP->CloseMatchSummary();
		}
	}
}

void AUTHUD::OpenMatchSummary()
{
	if (Cast<AUTDemoRecSpectator>(UTPlayerOwner))
	{
		return;
	}

	UUTLocalPlayer* UTLP = UTPlayerOwner ? Cast<UUTLocalPlayer>(UTPlayerOwner->Player) : NULL;
	AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GetGameState());
	if (UTLP && GS && !GS->IsPendingKillPending())
	{
		// temp check for testing, until match summary is ready.  Make sure same setting for client and server
		UTLP->OpenMatchSummary(GS);
		if (GS->GetMatchState() == MatchState::WaitingToStart)
		{
			UTLP->ShowMenu(TEXT(""));
		}
	}
}

void AUTHUD::PostRender()
{
	// @TODO FIXMESTEVE - need engine to also give pawn a chance to postrender so don't need this hack
	AUTRemoteRedeemer* Missile = UTPlayerOwner ? Cast<AUTRemoteRedeemer>(UTPlayerOwner->GetViewTarget()) : NULL;
	if (Missile && !UTPlayerOwner->IsBehindView())
	{
		Missile->PostRender(this, Canvas);
	}

	// Always sort the PlayerState array at the beginning of each frame
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		GS->SortPRIArray();
	}
	Super::PostRender();
}

void AUTHUD::CacheFonts()
{
	FText MessageText = NSLOCTEXT("AUTHUD", "FontCacheText", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';:-=+*(),.?!");
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	float YPos = 0.f;
	Canvas->DrawColor = FLinearColor::White;
	Canvas->DrawText(TinyFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(SmallFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(MediumFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(LargeFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(ScoreFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(NumberFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(HugeFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	bFontsCached = true;
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
		bool bScoreboardIsUp = bShowScores || bPreMatchScoreBoard || bForceScores;
		if (!bFontsCached)
		{
			CacheFonts();
		}
		if (PlayerOwner && PlayerOwner->PlayerState && PlayerOwner->PlayerState->bOnlySpectator)
		{
			AddSpectatorWidgets();
		}

		for (int32 WidgetIndex = 0; WidgetIndex < HudWidgets.Num(); WidgetIndex++)
		{
			// If we aren't hidden then set the canvas and render..
			if (HudWidgets[WidgetIndex] && !HudWidgets[WidgetIndex]->IsHidden() && !HudWidgets[WidgetIndex]->IsPendingKill())
			{
				HudWidgets[WidgetIndex]->PreDraw(RenderDelta, this, Canvas, Center);
				if (HudWidgets[WidgetIndex]->ShouldDraw(bScoreboardIsUp))
				{
					HudWidgets[WidgetIndex]->Draw(RenderDelta);
				}
				HudWidgets[WidgetIndex]->PostDraw(GetWorld()->GetTimeSeconds());
			}
		}

		if (UTPlayerOwner)
		{
			if (bScoreboardIsUp)
			{
				if (!UTPlayerOwner->CurrentlyViewedScorePS)
				{
					UTPlayerOwner->SetViewedScorePS(GetScorerPlayerState(), UTPlayerOwner->CurrentlyViewedStatsTab);
				}
			}
			else 
			{
				if (!UTPlayerOwner->bCurrentlyBehindView || !UTPlayerOwner->UTPlayerState || !UTPlayerOwner->UTPlayerState->bOnlySpectator)
				{
					DrawDamageIndicators();
				}
				UTPlayerOwner->SetViewedScorePS(NULL, 0);
				if (bDrawMinimap && UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator)
				{
					const float MapSize = float(Canvas->SizeY) * 0.75f;
					DrawMinimap(FColor(192, 192, 192, 210), MapSize, FVector2D(Canvas->SizeX - MapSize + MapSize*MinimapOffset.X, MapSize*MinimapOffset.Y));
				}
			}
		}
	}
}

FText AUTHUD::ConvertTime(FText Prefix, FText Suffix, int32 Seconds, bool bForceHours, bool bForceMinutes, bool bForceTwoDigits) const
{
	int32 Hours = Seconds / 3600;
	Seconds -= Hours * 3600;
	int32 Mins = Seconds / 60;
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

void AUTHUD::DrawNumber(int32 Number, float X, float Y, FLinearColor Color, float GlowOpacity, float Scale, int32 MinDigits, bool bRightAlign)
{
	FNumberFormattingOptions Opts;
	Opts.MinimumIntegralDigits = MinDigits;
	DrawString(FText::AsNumber(Number, &Opts), X, Y, bRightAlign ? ETextHorzPos::Right : ETextHorzPos::Left, ETextVertPos::Top, NumberFont, Color, Scale, true);
}

void AUTHUD::PawnDamaged(FVector HitLocation, int32 DamageAmount, bool bFriendlyFire)
{
	// Calculate the rotation 	
	AUTCharacter* UTC = Cast<AUTCharacter>(UTPlayerOwner->GetViewTarget());
	if (UTC != NULL && !UTC->IsDead() && DamageAmount > 0)	// If have a pawn and it's alive...
	{
		FVector CharacterLocation;
		FRotator CharacterRotation;

		UTC->GetActorEyesViewPoint(CharacterLocation, CharacterRotation);
		FVector HitSafeNormal = (HitLocation - CharacterLocation).GetSafeNormal2D();
		float Ang = FMath::Acos(FVector::DotProduct(CharacterRotation.Vector().GetSafeNormal2D(), HitSafeNormal)) * (180.0f / PI);

		// Figure out Left/Right....
		float FinalAng = ( FVector::DotProduct( FVector::CrossProduct(CharacterRotation.Vector(), FVector(0,0,1)), HitSafeNormal)) > 0 ? 360 - Ang : Ang;

		int32 BestIndex = 0;
		float BestTime = DamageIndicators[0].FadeTime;
		for (int32 i=0; i < MAX_DAMAGE_INDICATORS; i++)
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
	for (int32 i=0; i < DamageIndicators.Num(); i++)
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
}

FLinearColor AUTHUD::GetBaseHUDColor()
{
	return FLinearColor::White;
}

FLinearColor AUTHUD::GetWidgetTeamColor()
{
	// Add code to cache and return the team color if it's a team game

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL || (GS->bTeamGame && UTPlayerOwner && UTPlayerOwner->GetViewTarget()))
	{
		//return UTPlayerOwner->UTPlayerState->Team->TeamColor;
		APawn* HUDPawn = Cast<APawn>(UTPlayerOwner->GetViewTarget());
		AUTPlayerState* PS = HUDPawn ? Cast<AUTPlayerState>(HUDPawn->PlayerState) : NULL;
		if (PS != NULL)
		{
			return (PS->GetTeamNum() == 0) ? FLinearColor(0.15, 0.0, 0.0, 1.0) : FLinearColor(0.025, 0.025, 0.1, 1.0);
		}
	}

	return FLinearColor::Black;
}

void AUTHUD::CalcStanding()
{
	// NOTE: By here in the Hud rendering chain, the PlayerArray in the GameState has been sorted.
	if (CalcStandingTime == GetWorld()->GetTimeSeconds())
	{
		return;
	}
	CalcStandingTime = GetWorld()->GetTimeSeconds();
	Leaderboard.Empty();

	CurrentPlayerStanding = 0;
	CurrentPlayerSpread = 0;
	CurrentPlayerScore = 0;
	NumActualPlayers = 0;

	AUTPlayerState* MyPS = GetScorerPlayerState();

	if (!UTPlayerOwner || !MyPS) return;	// Quick out if not ready

	CurrentPlayerScore = int32(MyPS->Score);

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		// Build the leaderboard.
		for (int32 i=0;i<GameState->PlayerArray.Num();i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
			if (PS != NULL && !PS->bIsSpectator && !PS->bOnlySpectator)
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
		int32 MyIndex = Leaderboard.Find(MyPS);
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

float AUTHUD::GetCrosshairScale()
{
	// Apply pickup scaling
	float PickupScale = 1.f;
	const float WorldTime = GetWorld()->GetTimeSeconds();
	if (LastPickupTime > WorldTime - 0.3f)
	{
		if (LastPickupTime > WorldTime - 0.15f)
		{
			PickupScale = (1.f + 5.f * (WorldTime - LastPickupTime));
		}
		else
		{
			PickupScale = (1.f + 5.f * (LastPickupTime + 0.3f - WorldTime));
		}
	}

	if (Canvas != NULL)
	{
		PickupScale = PickupScale * Canvas->ClipX / 1920.f;
	}

	return PickupScale;
}

FLinearColor AUTHUD::GetCrosshairColor(FLinearColor CrosshairColor) const
{
	float TimeSinceHit = GetWorld()->TimeSeconds - LastConfirmedHitTime;
	if (TimeSinceHit < 0.4f)
	{
		CrosshairColor = FMath::Lerp<FLinearColor>(FLinearColor::Red, CrosshairColor, FMath::Lerp<float>(0.f, 1.f, FMath::Pow((GetWorld()->TimeSeconds - LastConfirmedHitTime) / 0.4f, 2.0f)));
	}
	return CrosshairColor;
}

FText AUTHUD::GetPlaceSuffix(int32 Value)
{
	switch (Value)
	{
		case 0: return FText::GetEmpty(); break;
		case 1:  return NSLOCTEXT("UTHUD","FirstPlaceSuffix","st"); break;
		case 2:  return NSLOCTEXT("UTHUD","SecondPlaceSuffix","nd"); break;
		case 3:  return NSLOCTEXT("UTHUD","ThirdPlaceSuffix","rd"); break;
		case 21:  return NSLOCTEXT("UTHUD", "FirstPlaceSuffix", "st"); break;
		case 22:  return NSLOCTEXT("UTHUD", "SecondPlaceSuffix", "nd"); break;
		case 23:  return NSLOCTEXT("UTHUD", "ThirdPlaceSuffix", "rd"); break;
		case 31:  return NSLOCTEXT("UTHUD", "FirstPlaceSuffix", "st"); break;
		case 32:  return NSLOCTEXT("UTHUD", "SecondPlaceSuffix", "nd"); break;
		default: return NSLOCTEXT("UTHUD", "NthPlaceSuffix", "th"); break;
	}

	return FText::GetEmpty();
}

UTexture2D* AUTHUD::ResolveFlag(AUTPlayerState* PS, FTextureUVs& UV)
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (PS && UTEngine)
	{
		FName FlagName = PS->CountryFlag;
		if (FlagName == NAME_None)
		{
			if (PS->bIsABot)
			{
				if (PS->Team)
				{
					// use team flag
					FlagName = (PS->Team->TeamIndex == 0) ? NAME_RedCountryFlag : NAME_BlueCountryFlag;
				}
				else
				{
					return nullptr;
				}
			}
		}
		UUTFlagInfo* FlagInfo = UTEngine->GetFlag(FlagName);
		if (FlagInfo != nullptr)
		{
			UV = FlagInfo->UV;
			return FlagInfo->GetTexture();
		}
	}
	return nullptr;
}

EInputMode::Type AUTHUD::GetInputMode_Implementation() const
{
	return EInputMode::EIM_None;
}

UUTCrosshair* AUTHUD::GetCrosshair(AUTWeapon* Weapon)
{
	FCrosshairInfo* CrosshairInfo = GetCrosshairInfo(Weapon);
	if (CrosshairInfo != nullptr)
	{
		for (int32 i = 0; i < LoadedCrosshairs.Num(); i++)
		{
			if (LoadedCrosshairs[i]->GetClass()->GetPathName() == CrosshairInfo->CrosshairClassName)
			{
				return LoadedCrosshairs[i];
			}
		}

		//Didn't find it so create a new one
		UClass* TestClass = LoadObject<UClass>(NULL, *CrosshairInfo->CrosshairClassName);
		if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(UUTCrosshair::StaticClass()))
		{
			UUTCrosshair* NewCrosshair = NewObject<UUTCrosshair>(this, TestClass);
			LoadedCrosshairs.Add(NewCrosshair);
		}
	}
	return nullptr;
}

FCrosshairInfo* AUTHUD::GetCrosshairInfo(AUTWeapon* Weapon)
{
	FString WeaponClass = (!bCustomWeaponCrosshairs || Weapon == nullptr) ? TEXT("Global") : Weapon->GetClass()->GetPathName();

	FCrosshairInfo* FoundInfo = CrosshairInfos.FindByPredicate([WeaponClass](const FCrosshairInfo& Info) { return Info.WeaponClassName == WeaponClass; });
	if (FoundInfo != nullptr)
	{
		return FoundInfo;
	}

	//Make a Global one if we couldn't find one
	if (WeaponClass == TEXT("Global"))
	{
		return &CrosshairInfos[CrosshairInfos.Add(FCrosshairInfo())];
	}
	else
	{
		//Create a new crosshair for this weapon based off the global one
		FCrosshairInfo NewInfo;
		FCrosshairInfo* GlobalCrosshair = CrosshairInfos.FindByPredicate([](const FCrosshairInfo& Info){ return Info.WeaponClassName == TEXT("Global"); });
		if (GlobalCrosshair != nullptr)
		{
			NewInfo = *GlobalCrosshair;
		}
		NewInfo.WeaponClassName = WeaponClass;
		return &CrosshairInfos[CrosshairInfos.Add(NewInfo)];
	}
}

void AUTHUD::CreateMinimapTexture()
{
	MinimapTexture = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), 1024, 1024);
	MinimapTexture->ClearColor = FLinearColor::Black;
	MinimapTexture->ClearColor.A = 0.f;
	MinimapTexture->OnCanvasRenderTargetUpdate.AddDynamic(this, &AUTHUD::UpdateMinimapTexture);
	MinimapTexture->UpdateResource();
}

void AUTHUD::CalcMinimapTransform(const FBox& LevelBox, int32 MapWidth, int32 MapHeight)
{
	const bool bLargerXAxis = LevelBox.GetExtent().X > LevelBox.GetExtent().Y;
	const float LevelRadius = bLargerXAxis ? LevelBox.GetExtent().X : LevelBox.GetExtent().Y;
	const float ScaleFactor = float(MapWidth) / (LevelRadius * 2.0f);
	const FVector CenteringAdjust = bLargerXAxis ? FVector(0.0f, (LevelBox.GetExtent().X - LevelBox.GetExtent().Y), 0.0f) : FVector((LevelBox.GetExtent().Y - LevelBox.GetExtent().X), 0.0f, 0.0f);
	MinimapOffset = FVector2D(0.f, 0.f);
	if (bLargerXAxis)
	{
		MinimapOffset.Y = 0.5f*(LevelBox.GetExtent().X - LevelBox.GetExtent().Y) / LevelBox.GetExtent().X;
	}
	else
	{
		MinimapOffset.X = 0.5f*(LevelBox.GetExtent().Y - LevelBox.GetExtent().X) / LevelBox.GetExtent().Y;
	}
	MinimapTransform = FTranslationMatrix(-LevelBox.Min + CenteringAdjust) * FScaleMatrix(FVector(ScaleFactor));
}

void AUTHUD::UpdateMinimapTexture(UCanvas* C, int32 Width, int32 Height)
{
	FBox LevelBox(0);
	AUTRecastNavMesh* NavMesh = GetUTNavData(GetWorld());
	if (NavMesh != NULL)
	{
		TMap<const UUTPathNode*, FNavMeshTriangleList> TriangleMap;
		NavMesh->GetNodeTriangleMap(TriangleMap);
		// calculate a bounding box for the level
		for (TMap<const UUTPathNode*, FNavMeshTriangleList>::TConstIterator It(TriangleMap); It; ++It)
		{
			const FNavMeshTriangleList& TriList = It.Value();
			for (const FVector& Vert : TriList.Verts)
			{
				LevelBox += Vert;
			}
		}
		if (LevelBox.IsValid)
		{
			LevelBox = LevelBox.ExpandBy(LevelBox.GetSize() * 0.01f); // extra so edges aren't right up against the texture
			CalcMinimapTransform(LevelBox, Width, Height);
			for (TMap<const UUTPathNode*, FNavMeshTriangleList>::TConstIterator It(TriangleMap); It; ++It)
			{
				const FNavMeshTriangleList& TriList = It.Value();

				for (const FNavMeshTriangleList::FTriangle& Tri : TriList.Triangles)
				{
					// don't draw triangles in water
					bool bInWater = false;
					FVector Verts[3] = { TriList.Verts[Tri.Indices[0]], TriList.Verts[Tri.Indices[1]], TriList.Verts[Tri.Indices[2]] };
					for (int32 i = 0; i < ARRAY_COUNT(Verts); i++)
					{
						UUTPathNode* Node = NavMesh->FindNearestNode(Verts[i], NavMesh->GetHumanPathSize().GetExtent());
						if (Node != NULL && Node->PhysicsVolume != NULL && Node->PhysicsVolume->bWaterVolume)
						{
							bInWater = true;
							break;
						}
						Verts[i] = MinimapTransform.TransformPosition(Verts[i]);
					}
					if (!bInWater)
					{
						FCanvasTriangleItem Item(FVector2D(Verts[0]), FVector2D(Verts[1]), FVector2D(Verts[2]), C->DefaultTexture->Resource);
						C->DrawItem(Item);
					}
				}
			}
		}
	}
	if (!LevelBox.IsValid)
	{
		// set minimap scale based on colliding geometry so map has some functionality without a working navmesh
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			TArray<UPrimitiveComponent*> Components;
			It->GetComponents(Components);
			for (UPrimitiveComponent* Prim : Components)
			{
				if (Prim->IsCollisionEnabled())
				{
					LevelBox += Prim->Bounds.GetBox();
				}
			}
		}
		LevelBox = LevelBox.ExpandBy(LevelBox.GetSize() * 0.01f); // extra so edges aren't right up against the texture
		CalcMinimapTransform(LevelBox, Width, Height);
	}
}

void AUTHUD::DrawMinimap(const FColor& DrawColor, float MapSize, FVector2D DrawPos)
{
	if (MinimapTexture == NULL)
	{
		CreateMinimapTexture();
	}

	FVector ScaleFactor(MapSize / MinimapTexture->GetSurfaceWidth(), MapSize / MinimapTexture->GetSurfaceHeight(), 1.0f);
	MapToScreen = FTranslationMatrix(FVector(DrawPos, 0.0f) / ScaleFactor) * FScaleMatrix(ScaleFactor);
	if (MinimapTexture != NULL)
	{
		Canvas->DrawColor = DrawColor;
		Canvas->DrawTile(MinimapTexture, MapToScreen.GetOrigin().X, MapToScreen.GetOrigin().Y, MapSize, MapSize, 0.0f, 0.0f, MinimapTexture->GetSurfaceWidth(), MinimapTexture->GetSurfaceHeight());
	}

	if (PlayerOwner && PlayerOwner->PlayerState && PlayerOwner->PlayerState->bOnlySpectator)
	{
		DrawMinimapSpectatorIcons();
	}
}

void AUTHUD::DrawMinimapSpectatorIcons()
{
	const float RenderScale = float(Canvas->SizeY) / 1080.0f;
	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
	{
		AUTCharacter* UTChar = Cast<AUTCharacter>(*Iterator);
		if (UTChar)
		{
			FVector2D Pos(WorldToMapToScreen(UTChar->GetActorLocation()));
			if (UTChar->bTearOff)
			{
				// Draw skull at location
				DrawMinimapIcon(HUDAtlas, Pos, FVector2D(20.f, 20.f), FVector2D(725.f, 0.f), FVector2D(28.f, 36.f), FLinearColor::White, true);
			}
			else
			{
				// draw team colored dot at location
				AUTPlayerState* PS = Cast<AUTPlayerState>(UTChar->PlayerState);
				FLinearColor PlayerColor = (PS && PS->Team) ? PS->Team->TeamColor : FLinearColor::Green;
				PlayerColor.A = 0.5f;
				Canvas->K2_DrawTexture(PlayerMinimapTexture, Pos - FVector2D(10.0f * RenderScale, 10.0f * RenderScale), FVector2D(20.0f, 20.0f) * RenderScale, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), PlayerColor, BLEND_Translucent, UTChar->GetActorRotation().Yaw + 90.0f);

				if (Cast<AUTPlayerController>(PlayerOwner) && (Cast<AUTPlayerController>(PlayerOwner)->LastSpectatedPlayerId == PS->SpectatingID))
				{
					Canvas->DrawColor = FColor(255, 255, 0, 255);
					Canvas->DrawTile(SelectedPlayerTexture, Pos.X - 12.0f * RenderScale, Pos.Y - 12.0f * RenderScale, 24.0f * RenderScale, 24.0f * RenderScale, 0.0f, 0.0f, SelectedPlayerTexture->GetSurfaceWidth(), SelectedPlayerTexture->GetSurfaceHeight());
				}
			}
		}
	}
}

void AUTHUD::DrawMinimapIcon(UTexture2D* Texture, FVector2D Pos, FVector2D DrawSize, FVector2D UV, FVector2D UVL, FLinearColor DrawColor, bool bDropShadow)
{
	const float RenderScale = float(Canvas->SizeY) / 1080.0f;
	float Height = DrawSize.X * RenderScale;
	float Width = DrawSize.Y * RenderScale;
	FVector2D RenderPos = FVector2D(Pos.X - (Width * 0.5f), Pos.Y - (Height * 0.5f));
	float U = UV.X / Texture->Resource->GetSizeX();
	float V = UV.Y / Texture->Resource->GetSizeY();;
	float UL = U + (UVL.X / Texture->Resource->GetSizeX());
	float VL = V + (UVL.Y / Texture->Resource->GetSizeY());
	if (bDropShadow)
	{
		FCanvasTileItem ImageItemShadow(FVector2D(RenderPos.X - 1.f, RenderPos.Y - 1.f), Texture->Resource, FVector2D(Width, Height), FVector2D(U, V), FVector2D(UL, VL), FLinearColor::Black);
		ImageItemShadow.Rotation = FRotator(0.f, 0.f, 0.f);
		ImageItemShadow.PivotPoint = FVector2D(0.f, 0.f);
		ImageItemShadow.BlendMode = ESimpleElementBlendMode::SE_BLEND_Translucent;
		Canvas->DrawItem(ImageItemShadow);
	}
	FCanvasTileItem ImageItem(RenderPos, Texture->Resource, FVector2D(Width, Height), FVector2D(U, V), FVector2D(UL, VL), DrawColor);
	ImageItem.Rotation = FRotator(0.f, 0.f, 0.f);
	ImageItem.PivotPoint = FVector2D(0.f, 0.f);
	ImageItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_Translucent;
	Canvas->DrawItem(ImageItem);
}
