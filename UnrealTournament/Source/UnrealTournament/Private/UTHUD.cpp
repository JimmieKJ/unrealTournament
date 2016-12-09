// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTProfileSettings.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTHUDWidgetMessage_ConsoleMessages.h"
#include "UTHUDWidget_WeaponInfo.h"
#include "UTHUDWidget_WeaponCrosshair.h"
#include "UTHUDWidget_Spectator.h"
#include "UTHUDWidget_WeaponBar.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTHUDWidget_WeaponCrosshair.h"
#include "UTHUDWidgetMessage_KillIconMessages.h"
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
#include "UTGameVolume.h"
#include "UTRadialMenu.h"
#include "UTRadialMenu_Coms.h"
#include "UTRadialMenu_WeaponWheel.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemUtils.h"
#include "UTUMGHudWidget.h"
#include "UTGameMessage.h"
#include "UTLineUpZone.h"
#include "UTLineUpHelper.h"
#include "UTRallyPoint.h"
#include "UTFlagRunGameState.h"
#include "UTHUDWidgetAnnouncements.h"

static FName NAME_Intensity(TEXT("Intensity"));

AUTHUD::AUTHUD(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetOpacity = 1.0f;

	// Set the crosshair texture
	static ConstructorHelpers::FObjectFinder<UTexture2D> CrosshairTexObj(TEXT("Texture2D'/Game/RestrictedAssets/Textures/crosshair.crosshair'"));
	DefaultCrosshairTex = CrosshairTexObj.Object;

	static ConstructorHelpers::FObjectFinder<UFont> ChFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fnt_Chat.fnt_Chat'"));
	ChatFont = ChFont.Object;

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

	// non-proportional FIXMESTEVE need better font and just numbers
	static ConstructorHelpers::FObjectFinder<UFont> CFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Clock.fntScoreboard_Clock'"));
	NumberFont = CFont.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> OldDamageIndicatorObj(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_DamageDir.UI_HUD_DamageDir'"));
	DamageIndicatorTexture = OldDamageIndicatorObj.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> HUDTex(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas01.HUDAtlas01'"));
	HUDAtlas = HUDTex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> HUDTex3(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas03.HUDAtlas03'"));
	HUDAtlas3 = HUDTex3.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> CharacterPortraitTex1(TEXT("Texture2D'/Game/RestrictedAssets/UI/CharacterPortraits/CharacterPortraitAtlas1.CharacterPortraitAtlas1'"));
	CharacterPortraitAtlas = CharacterPortraitTex1.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> PlayerDirectionTextureObject(TEXT("/Game/RestrictedAssets/UI/MiniMap/Minimap_PS_BG.Minimap_PS_BG"));
	PlayerMinimapTexture = PlayerDirectionTextureObject.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> SelectedPlayerTextureObject(TEXT("/Game/RestrictedAssets/Weapons/Sniper/Assets/TargetCircle.TargetCircle"));
	SelectedPlayerTexture = SelectedPlayerTextureObject.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> KillSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Gameplay/A_Stinger_Kill01_Cue.A_Stinger_Kill01_Cue'"));
	KillSound = KillSoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> ScoreboardTex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	ScoreboardAtlas = ScoreboardTex.Object;

	SpawnHelpTextBG.U = 4;
	SpawnHelpTextBG.V = 2;
	SpawnHelpTextBG.UL = 124;
	SpawnHelpTextBG.VL = 128;
	SpawnHelpTextBG.Texture = ScoreboardAtlas;

	LastKillTime = -100.f;
	LastConfirmedHitTime = -100.0f;
	LastPickupTime = -100.f;
	bFontsCached = false;
	bShowOverlays = true;

	TeamIconUV[0] = FVector2D(257.f, 940.f);
	TeamIconUV[1] = FVector2D(333.f, 940.f);

	bShowUTHUD = true;

	TimerHours = NSLOCTEXT("UTHUD", "TIMERHOURS", "{Prefix}{Hours}:{Minutes}:{Seconds}{Suffix}");
	TimerMinutes = NSLOCTEXT("UTHUD", "TIMERMINUTES", "{Prefix}{Minutes}:{Seconds}{Suffix}");
	TimerSeconds = NSLOCTEXT("UTHUD", "TIMERSECONDS", "{Prefix}{Seconds}{Suffix}");
	SuffixFirst = NSLOCTEXT("UTHUD", "FirstPlaceSuffix", "st");
	SuffixSecond = NSLOCTEXT("UTHUD", "SecondPlaceSuffix", "nd");
	SuffixThird = NSLOCTEXT("UTHUD", "ThirdPlaceSuffix", "rd");
	SuffixNth = NSLOCTEXT("UTHUD", "NthPlaceSuffix", "th");

	CachedProfileSettings = nullptr;
	BuildText = NSLOCTEXT("UTHUD", "info", "PRE-ALPHA Build 0.1.6");
	bShowVoiceDebug = false;
	bDrawDamageNumbers = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DamageScreenMatObject(TEXT("/Game/RestrictedAssets/Blueprints/WIP/Nick/CameraAnims/HitScreenEffect.HitScreenEffect"));
	DamageScreenMat = DamageScreenMatObject.Object;

	ScoreboardKillFeedPosition = FVector2D(0.f, 0.635f);

	MiniMapIconAlpha = 1.f;
	MiniMapIconMuting = 0.8f;
}

void AUTHUD::Destroyed()
{
	Super::Destroyed();
	CachedProfileSettings = nullptr;

	UMGHudWidgetStack.Empty();
}

void AUTHUD::ClearIndicators()
{
	LastConfirmedHitTime = -100.0f;
	LastPickupTime = -100.f;
	for (int32 i = 0; i < DamageIndicators.Num(); i++)
	{
		DamageIndicators[i].FadeTime = 0.f;
	}
}

bool AUTHUD::VerifyProfileSettings()
{
	if (CachedProfileSettings == nullptr)
	{
		UUTLocalPlayer* UTLP = UTPlayerOwner ? Cast<UUTLocalPlayer>(UTPlayerOwner->Player) : NULL;
		CachedProfileSettings = UTLP ? UTLP->GetProfileSettings() : nullptr;
	}
	return CachedProfileSettings != nullptr;
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

	// preload all known required crosshairs for this map
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(AUTWeapon::StaticClass()))
		{
			AUTWeapon* DefaultWeapon = It->GetDefaultObject<AUTWeapon>();
			if (DefaultWeapon != nullptr)
			{
				FWeaponCustomizationInfo Info;
				GetCrosshairForWeapon(DefaultWeapon->WeaponCustomizationTag, Info);
			}
		}
	}

	AddSpectatorWidgets();

	// Add the Coms Menu
	ComsMenu = Cast<UUTRadialMenu_Coms>(AddHudWidget(UUTRadialMenu_Coms::StaticClass()));
	WeaponWheel = Cast<UUTRadialMenu_WeaponWheel>(AddHudWidget(UUTRadialMenu_WeaponWheel::StaticClass()));
	RadialMenus.Add(ComsMenu);
	RadialMenus.Add(WeaponWheel);

	if (DamageScreenMat != nullptr)
	{
		DamageScreenMID = UMaterialInstanceDynamic::Create(DamageScreenMat, this);
	}
}

void AUTHUD::AddSpectatorWidgets()
{
	// Parse the widgets found in the ini
	for (int32 i = 0; i < SpectatorHudWidgetClasses.Num(); i++)
	{
		BuildHudWidget(*SpectatorHudWidgetClasses[i]);
	}
}

void AUTHUD::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UTPlayerOwner = Cast<AUTPlayerController>(GetOwner());

	// Grab all of the available crosshairs...

	TArray<FAssetData> AssetList;
	GetAllBlueprintAssetData(UUTCrosshair::StaticClass(), AssetList);
	for (const FAssetData& Asset : AssetList)
	{
		static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
		const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
		UClass* CrosshairClass = LoadObject<UClass>(NULL, **ClassPath);
		if (CrosshairClass != nullptr)
		{
			UUTCrosshair* Crosshair = NewObject<UUTCrosshair>(this, CrosshairClass, NAME_None, RF_NoFlags);
			if (Crosshair && Crosshair->CrosshairTag != NAME_None)
			{
				Crosshairs.Add(Crosshair->CrosshairTag, Crosshair);
			}
		}
	}
}

void AUTHUD::DrawActorOverlays(FVector Viewpoint, FRotator ViewRotation)
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	bool bShouldShowSkulls = UTGameState && UTGameState->bTeamGame && ((UTGameState->IsMatchInProgress() && !UTGameState->IsMatchIntermission()) || (UTGameState->GetMatchState() == MatchState::WaitingToStart));
	if (bShouldShowSkulls)
	{
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PlayerState && !PlayerState->bOnlySpectator)
			{
				PlayerState->bPawnWasPostRendered = false;
			}
		}
	}

	// determine rendered camera position
	FVector ViewDir = ViewRotation.Vector();
	int32 i = 0;
	while (i < PostRenderedActors.Num())
	{
		if (PostRenderedActors[i] != NULL)
		{
			PostRenderedActors[i]->PostRenderFor(PlayerOwner, Canvas, Viewpoint, ViewDir);
			i++;
		}
		else
		{
			PostRenderedActors.RemoveAt(i, 1);
		}
	}

	if (bShouldShowSkulls)
	{
		for (i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PlayerState && !PlayerState->bOnlySpectator && !PlayerState->bPawnWasPostRendered && !PlayerState->LastPostRenderedLocation.IsZero() && (GetWorld()->GetTimeSeconds() - PlayerState->PawnPostRenderedTime < 5.f))
			{
				PlayerState->PostRenderFor(PlayerOwner, Canvas, Viewpoint, ViewDir);
			}
			else if (PlayerState)
			{
				PlayerState->bPawnWasPostRendered = true;
			}
		}
	}
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
	case -1: return ChatFont;
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
	if (Cast<UUTHUDWidgetAnnouncements>(Widget))
	{
		AnnouncementWidget = Cast<UUTHUDWidgetAnnouncements>(Widget);
	}
	if (KillIconWidget == nullptr)
	{
		KillIconWidget = Cast<UUTHUDWidgetMessage_KillIconMessages>(Widget);
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

void AUTHUD::UpdateKeyMappings(bool bForceUpdate)
{
	if (!bKeyMappingsSet || bForceUpdate)
	{
		bKeyMappingsSet = true;
		BoostLabel = FindKeyMappingTo("ActivateSpecial");
		RallyLabel = FindKeyMappingTo("RequestRally");
		ShowScoresLabel = FindKeyMappingTo("ShowScores");
	}
}

FText AUTHUD::FindKeyMappingTo(FName InActionName)
{
	UUTProfileSettings* ProfileSettings;
	ProfileSettings = UTPlayerOwner->GetProfileSettings();
	if (ProfileSettings)
	{
		const FKeyConfigurationInfo* GameAction = ProfileSettings->FindGameAction(InActionName);
		if (GameAction != nullptr)
		{
			if (GameAction->PrimaryKey != FKey()) return GameAction->PrimaryKey.GetDisplayName();
			if (GameAction->SecondaryKey != FKey()) return GameAction->SecondaryKey.GetDisplayName();
			if (GameAction->GamepadKey != FKey()) return GameAction->GamepadKey.GetDisplayName();
		}
	}

	return FText::FromString(TEXT("<none>"));
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
	bShowScores = bShow;
}

void AUTHUD::NotifyMatchStateChange()
{
	// FIXMESTEVE - in playerintro mode, open match summary if not open (option for UTLP openmatchsummary)
	UUTLocalPlayer* UTLP = UTPlayerOwner ? Cast<UUTLocalPlayer>(UTPlayerOwner->Player) : NULL;
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (UTLP && GS && !GS->IsPendingKillPending())
	{
		if (GS->GetMatchState() == MatchState::InProgress)
		{
			bShowScores = false;
			bForceScores = false;
		}
		else if (GS->GetMatchState() == MatchState::WaitingPostMatch)
		{
			if (GS->GameModeClass != nullptr)
			{
				return;
			}
			const AUTGameMode* DefaultGame = Cast<AUTGameMode>(GS->GetDefaultGameMode());
			float MatchSummaryDelay = DefaultGame ? DefaultGame->EndScoreboardDelay + DefaultGame->MainScoreboardDisplayTime + DefaultGame->ScoringPlaysDisplayTime : 10.f;
			GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD::OpenMatchSummary, MatchSummaryDelay*GetActorTimeDilation(), false);
		}
		else if (GS->GetMatchState() == MatchState::WaitingToStart)
		{
			// GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD::OpenMatchSummary, 0.5f, false);
		}
		else if (GS->GetMatchState() == MatchState::PlayerIntro)
		{
			bool bShouldUseLineUp = false;
			if (GS->LineUpHelper)
			{
				AUTLineUpZone* IntroZoneFound = AUTLineUpHelper::GetAppropriateSpawnList(GetWorld(), LineUpTypes::Intro);
				if (IntroZoneFound)
				{
					bShouldUseLineUp = true;
				}
			}

			//if we can't line up, use old method
			if (!bShouldUseLineUp)
			{
				if (UTPlayerOwner->UTPlayerState && UTPlayerOwner->UTPlayerState->bIsWarmingUp)
				{
					UTPlayerOwner->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(), 16, nullptr, nullptr, nullptr);
				}
				GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD::OpenMatchSummary, 1.7f, false);
			}
		}
		else if (GS->GetMatchState() != MatchState::MapVoteHappening)
		{
			ToggleScoreboard(false);
			UTLP->HideMenu();

			if (UTLP->HasChatText() && UTPlayerOwner && UTPlayerOwner->UTPlayerState)
			{
				UTLP->ShowQuickChat(UTPlayerOwner->UTPlayerState->ChatDestination);
			}
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
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (UTLP && GS && !GS->IsPendingKillPending())
	{
		if (PlayerOwner && PlayerOwner->PlayerState && !PlayerOwner->PlayerState->bOnlySpectator)
		{
			UTLP->CloseSpectatorWindow();
		}
		UTLP->ShowMenu(TEXT("forcesummary"));
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


	//DrawString(FText::Format( NSLOCTEXT("a","b","InputMode: {0}"),  FText::AsNumber(Cast<AUTBasePlayerController>(PlayerOwner)->InputMode)), 0, 0, ETextHorzPos::Left, ETextVertPos::Top, SmallFont, FLinearColor::White, 1.0, true);
	//Canvas->SetDrawColor(255,0,0,255);

}

void AUTHUD::CacheFonts()
{
	FText MessageText = NSLOCTEXT("AUTHUD", "FontCacheText", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';:-=+*(),.?!");
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	float YPos = 0.f;
	Canvas->DrawColor = FLinearColor::White.ToFColor(false);
	Canvas->DrawText(TinyFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(SmallFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(MediumFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(LargeFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(NumberFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	//YPos += 0.1f*Canvas->ClipY;
	Canvas->DrawText(HugeFont, MessageText, 0.f, YPos, 0.1f, 0.1f, TextRenderInfo);
	bFontsCached = true;

	float YL;
	Canvas->TextSize(TinyFont, BuildText.ToString(), BuildTextWidth, YL, 1.f, 1.f);
}

void AUTHUD::ShowHUD()
{
	bShowUTHUD = !bShowUTHUD;
	bShowHUD = bShowUTHUD;
	//Super::ShowHUD();
}

bool AUTHUD::ScoreboardIsUp()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	bool bPreMatchScoreBoard = (GS && !GS->HasMatchStarted() && !GS->IsMatchInCountdown()) && (!UTPlayerOwner || !UTPlayerOwner->UTPlayerState || !UTPlayerOwner->UTPlayerState->bIsWarmingUp);
	bShowScoresWhileDead = bShowScoresWhileDead && GS && GS->IsMatchInProgress() && !GS->IsMatchIntermission() && UTPlayerOwner && !UTPlayerOwner->GetPawn() && !UTPlayerOwner->IsInState(NAME_Spectating);
	return bShowScores || bPreMatchScoreBoard || bForceScores || bShowScoresWhileDead;
}

void AUTHUD::DrawHUD()
{
	// FIXMESTEVE - once bShowHUD is not config, can just use it without bShowUTHUD and bCinematicMode
	if (!bShowUTHUD || UTPlayerOwner == nullptr || (!bShowHUD && UTPlayerOwner && UTPlayerOwner->bCinematicMode))
	{
		return;
	}

	if (!IsPendingKillPending() && !IsPendingKill())
	{
		Super::DrawHUD();

		// find center of the Canvas
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

		bool bScoreboardIsUp = ScoreboardIsUp();
		if (!bFontsCached)
		{
			CacheFonts();
		}
		if (PlayerOwner && PlayerOwner->PlayerState && PlayerOwner->PlayerState->bOnlySpectator)
		{
			UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(PlayerOwner->Player);
			if (UTLP)
			{
				UTLP->OpenSpectatorWindow();
			}
		}

		if (DamageScreenMID != nullptr)
		{
			float Intensity = 0.0f;
			for (const FDamageHudIndicator& Indicator : DamageIndicators)
			{
				if (Indicator.FadeTime > 0.0f && Indicator.DamageAmount > 0.0f)
				{
					Intensity = FMath::Max<float>(Intensity, FMath::Min<float>(1.0f, Indicator.FadeTime / DAMAGE_FADE_DURATION));
				}
			}
			if (Cast<AUTCharacter>(PlayerOwner->GetViewTarget()) != nullptr && PlayerOwner->GetViewTarget()->bTearOff)
			{
				Intensity = FMath::Max<float>(Intensity, 0.5f);
			}
			if (Intensity > 0.0f)
			{
				DamageScreenMID->SetScalarParameterValue(NAME_Intensity, Intensity);
				DrawMaterial(DamageScreenMID, 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, 0.0f, 0.0f, 1.0f, 1.0f);
			}
		}

		UpdateKeyMappings(false);
		if (KillIconWidget)
		{
			KillIconWidget->ScreenPosition = bScoreboardIsUp ? ScoreboardKillFeedPosition : FVector2D(0.0f, 0.0f);
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
				if (!UTPlayerOwner->IsBehindView() || !UTPlayerOwner->UTPlayerState || !UTPlayerOwner->UTPlayerState->bOnlySpectator)
				{
					DrawDamageIndicators();
				}
				if (SpectatorSlideOutWidget && SpectatorSlideOutWidget->bShowingStats)
				{
					if (UTPlayerOwner->CurrentlyViewedScorePS != GetScorerPlayerState())
					{
						UTPlayerOwner->CurrentlyViewedStatsTab = 1;
						UTPlayerOwner->SetViewedScorePS(GetScorerPlayerState(), UTPlayerOwner->CurrentlyViewedStatsTab);
					}
				}
				else
				{
					UTPlayerOwner->SetViewedScorePS(NULL, 0);
				}
				if (ShouldDrawMinimap())
				{
					bool bSpectatingMinimap = UTPlayerOwner->UTPlayerState && (UTPlayerOwner->UTPlayerState->bOnlySpectator || UTPlayerOwner->UTPlayerState->bOutOfLives);
					float MapScale = bSpectatingMinimap ? 0.75f : 0.25f;
					const float MapSize = float(Canvas->SizeY) * MapScale;
					uint8 MapAlpha = bSpectatingMinimap ? 210 : 100;
					const float YOffsetToMaintainPosition = MapSize * MinimapOffset.Y * -.5f;
					DrawMinimap(FColor(192, 192, 192, MapAlpha), MapSize, FVector2D(Canvas->SizeX - MapSize + MapSize*MinimapOffset.X, YOffsetToMaintainPosition));
				}
				if (bDrawDamageNumbers)
				{
					DrawDamageNumbers();
				}
			}

			// TODO: temp delayed pickup display, formalize if we keep
			static FName NAME_DelayedTouch(TEXT("DelayedTouch"));
			AUTCharacter* UTC = Cast<AUTCharacter>(UTPlayerOwner->GetViewTarget());
			if (UTC != NULL)
			{
				TArray<AActor*> Touching;
				UTC->GetCapsuleComponent()->GetOverlappingActors(Touching, AUTPickup::StaticClass());
				for (AActor* A : Touching)
				{
					float TotalTime, ElapsedTime;
					if (A->FindFunction(NAME_DelayedTouch) && IsTimerActiveUFunc(A, NAME_DelayedTouch, &TotalTime, &ElapsedTime))
					{
						TArray<AActor*> PickupClaims;
						A->GetOverlappingActors(PickupClaims, APawn::StaticClass());
						if (PickupClaims.Num() <= 1)
						{
							Canvas->DrawColor = FColor::White;
							FVector2D Size(256.0f, 64.0f);
							FVector2D Pos((Canvas->SizeX - Size.X) * 0.5f, Canvas->SizeY * 0.4f - Size.Y * 0.5f);
							Canvas->K2_DrawBox(Pos, Size, 4.0f);
							Canvas->DrawTile(Canvas->DefaultTexture, Pos.X, Pos.Y, Size.X * ElapsedTime / TotalTime, Size.Y, 0.0f, 0.0f, 1.0f, 1.0f, BLEND_Opaque);
						}
					}
				}
			}
		}

		// tick down damage indicators
		for (FDamageHudIndicator& Indicator : DamageIndicators)
		{
			if (Indicator.FadeTime > 0.0f)
			{
				Indicator.FadeTime -= RenderDelta;
			}
		}
	}

	if (bShowVoiceDebug)
	{
		float TextScale = Canvas->ClipY / 1080.0f;
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(GetWorld());
		if (VoiceInt.IsValid())
		{
			FString VoiceDebugString = 	VoiceInt->GetVoiceDebugState();
			if (!VoiceDebugString.IsEmpty())
			{
				TArray<FString> VDLines;
				VoiceDebugString.ParseIntoArray(VDLines,TEXT("\n"), false);
				FVector2D Pos = FVector2D(10, Canvas->ClipY * 0.2f);
				for (int32 i=0 ; i < VDLines.Num(); i++)
				{
					DrawString(FText::FromString(VDLines[i]), Pos.X, Pos.Y, ETextHorzPos::Left, ETextVertPos::Top, TinyFont, FLinearColor::White, TextScale, true);
					Pos.Y += TinyFont->GetMaxCharHeight() * TextScale;
				}
			}
		}
	}

	CachedProfileSettings = nullptr;
	DrawWatermark();
}

void AUTHUD::DrawKillSkulls()
{
	float TimeSinceKill = GetWorld()->GetTimeSeconds() - LastKillTime;
	float SkullDisplayTime = (LastMultiKillCount > 1) ? 1.1f : 0.8f;
	if ((TimeSinceKill < SkullDisplayTime) && GetDrawHUDKillIconMsg())
	{
		float SkullSmallTime = (LastMultiKillCount > 1) ? 0.5f : 0.2f;
		float DrawSize = 32.f * (1.f + FMath::Clamp(1.5f*(TimeSinceKill - SkullSmallTime) / SkullDisplayTime, 0.f, 1.f));
		FLinearColor SkullColor = FLinearColor::White;
		float DrawOpacity = 255.f * FMath::Clamp(0.7f - 0.6f*(TimeSinceKill - SkullSmallTime) / (SkullDisplayTime - SkullSmallTime), 0.f, 1.f);
		int32 NumSkulls = (LastMultiKillCount > 1) ? LastMultiKillCount : 1;
		float StartPos = -0.5f * DrawSize * NumSkulls;
		const float RenderScale = float(Canvas->SizeY) / 1080.0f;

		Canvas->DrawColor = FColor(255, 255, 255, uint32(DrawOpacity));
		for (int32 i = 0; i < NumSkulls; i++)
		{
			Canvas->DrawTile(HUDAtlas, 0.5f*Canvas->ClipX + StartPos*RenderScale, 0.5f*Canvas->ClipY - 2.f*DrawSize*RenderScale, DrawSize * RenderScale, DrawSize*RenderScale, 725, 0, 28, 36);
			StartPos += 1.1f * DrawSize;
		}
	}
	else
	{
		LastMultiKillCount = 0;
	}
}

void AUTHUD::DrawWatermark()
{
	float RenderScale = Canvas->ClipX / 1920.0f;
	FVector2D Size = FVector2D(150.0f * RenderScale, 49.0f * RenderScale);
	FVector2D Position = FVector2D(Canvas->ClipX - Size.X - 10.0f * RenderScale, Canvas->ClipY - Size.Y - 70.0f * RenderScale);
	Canvas->DrawColor = FColor(255,255,255,64);
	Canvas->DrawTile(ScoreboardAtlas, Position.X, Position.Y, Size.X, Size.Y, 162.0f, 14.0f, 301.0f, 98.0f);

	Position.X = Canvas->ClipX - BuildTextWidth*RenderScale - 10.0f * RenderScale;
	Position.Y += Size.Y * 0.85f;
	Canvas->DrawText(TinyFont, BuildText, Position.X, Position.Y, RenderScale, RenderScale);
}

bool AUTHUD::ShouldDrawMinimap() 
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();

	return  (bDrawMinimap && GS && GS->AllowMinimapFor(UTPlayerOwner->UTPlayerState));
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
		return FText::Format(TimerHours, Args);
	}
	else if (bDisplayMinutes)
	{
		return FText::Format(TimerMinutes, Args);
	}
	else
	{
		return FText::Format(TimerSeconds, Args);
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

void AUTHUD::ClientRestart()
{
}

void AUTHUD::PawnDamaged(uint8 ShotDirYaw, int32 DamageAmount, bool bFriendlyFire, TSubclassOf<class UDamageType> DamageTypeClass)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(UTPlayerOwner->GetViewTarget());

	// Calculate the rotation 	
	if (UTC != NULL && !UTC->IsDead() && DamageAmount > 0)	// If have a pawn and it's alive...
	{
		// Figure out Left/Right....
		float FinalAng = (DamageTypeClass && DamageTypeClass->GetDefaultObject<UDamageType>()->bCausedByWorld) ? 0.f : FRotator::DecompressAxisFromByte(ShotDirYaw) - UTC->GetActorRotation().Yaw;
		int32 BestIndex = 0;
		float BestTime = DamageIndicators[0].FadeTime;
		for (int32 i = 0; i < MAX_DAMAGE_INDICATORS; i++)
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
		DamageIndicators[BestIndex].FadeTime = DAMAGE_FADE_DURATION * FMath::Clamp(0.025f*DamageAmount, 0.7f, 2.f);
		DamageIndicators[BestIndex].RotationAngle = FinalAng + 180.f;
		DamageIndicators[BestIndex].bFriendlyFire = bFriendlyFire;
		DamageIndicators[BestIndex].DamageAmount = DamageAmount;

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
			DrawColor.A = 1.f * (DamageIndicators[i].FadeTime / DAMAGE_FADE_DURATION);

			float Size = 384 * (Canvas->ClipY / 720.0f);
			float Half = Size * 0.5f;

			FCanvasTileItem ImageItem(FVector2D((Canvas->ClipX * 0.5f) - Half, (Canvas->ClipY * 0.5f) - Half), DamageIndicatorTexture->Resource, FVector2D(Size, Size), FVector2D(0.f,0.f), FVector2D(1.f,1.f), DrawColor);
			ImageItem.Rotation = FRotator(0.f,DamageIndicators[i].RotationAngle,0.f);
			ImageItem.PivotPoint = FVector2D(0.5f,0.5f);
			ImageItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_Translucent;
			Canvas->DrawItem( ImageItem );
		}
	}
}

void AUTHUD::CausedDamage(APawn* HitPawn, int32 Damage)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if ((HitPawn != UTPlayerOwner->GetViewTarget()) && (GS == NULL || !GS->OnSameTeam(HitPawn, PlayerOwner)))
	{
		LastConfirmedHitDamage = (GetWorld()->GetTimeSeconds() - LastConfirmedHitTime < 0.05f) ? LastConfirmedHitDamage + Damage : Damage;
		LastConfirmedHitTime = GetWorld()->TimeSeconds;
		AUTCharacter* Char = Cast<AUTCharacter>(HitPawn);
		LastConfirmedHitWasAKill = (Char && (Char->IsDead() || Char->Health <= 0));

		if (bDrawDamageNumbers && (HitPawn != nullptr))
		{
			// add to current hit if there
			for (int32 i = 0; i < DamageNumbers.Num(); i++)
			{
				if ((DamageNumbers[i].DamagedPawn == HitPawn) && (GetWorld()->GetTimeSeconds() - DamageNumbers[i].DamageTime < 0.04f))
				{
					DamageNumbers[i].DamageAmount = FMath::Min(255, Damage + int32(DamageNumbers[i].DamageAmount));
					return;
				}
			}
			// save amount, scale , 2D location
			float HalfHeight = Cast<ACharacter>(HitPawn) ? 1.15f * ((ACharacter *)(HitPawn))->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 0.f;
			DamageNumbers.Add(FEnemyDamageNumber(HitPawn, GetWorld()->GetTimeSeconds(), FMath::Min(Damage, 255), HitPawn->GetActorLocation() + FVector(0.f, 0.f, HalfHeight), 0.75f));
		}
	}
}

void AUTHUD::DrawDamageNumbers()
{
#if !UE_BUILD_SHIPPING
	//	UE_LOG(UT, Warning, TEXT("DrawDamageNumbers, numbers %d"), DamageNumbers.Num());
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	Canvas->DrawColor = FColor::Red;

	for (int32 i = 0; i < DamageNumbers.Num(); i++)
	{
		DamageNumbers[i].Scale = DamageNumbers[i].Scale + 2.3f * GetWorld()->DeltaTimeSeconds;
		if (DamageNumbers[i].Scale > 1.7f)
		{
			DamageNumbers.RemoveAt(i, 1);
			i--;
		}
		else
		{
			Canvas->DrawColor.A = 255.f * (1.f - 0.45f * DamageNumbers[i].Scale);
			FVector ScreenPosition = Canvas->Project(DamageNumbers[i].WorldPosition);
			float XL, YL;
			FString DamageString = FString::Printf(TEXT("%d"), DamageNumbers[i].DamageAmount);
			Canvas->TextSize(MediumFont, DamageString, XL, YL, DamageNumbers[i].Scale, DamageNumbers[i].Scale);
			Canvas->DrawText(MediumFont, DamageString, ScreenPosition.X - 0.5f*XL, ScreenPosition.Y - 0.5f*YL, DamageNumbers[i].Scale, DamageNumbers[i].Scale, TextRenderInfo);
		}
	}
#endif
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
	return CrosshairColor;
}

FText AUTHUD::GetPlaceSuffix(int32 Value)
{
	switch (Value)
	{
		case 0: return FText::GetEmpty(); break;
		case 1:  return SuffixFirst; break;
		case 2:  return SuffixSecond; break;
		case 3:  return SuffixThird; break;
		case 21:  return SuffixFirst; break;
		case 22:  return SuffixSecond; break;
		case 23:  return SuffixThird; break;
		case 31:  return SuffixFirst; break;
		case 32:  return SuffixSecond; break;
		default: return SuffixNth; break;
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
	if (UTPlayerOwner != nullptr)
	{
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		if (GameState == nullptr || GameState->GetMatchState() == MatchState::InProgress)
		{
			AUTPlayerState* UTPlayerState = UTPlayerOwner->UTPlayerState;
			if (UTPlayerState && (UTPlayerState->bOnlySpectator || UTPlayerState->bOutOfLives) )
			{
				if (UTPlayerOwner->bSpectatorMouseChangesView)
				{
					return EInputMode::EIM_GameOnly;
				}
				else
				{
					return EInputMode::EIM_UIOnly;
				}
			}
		}
	}
	return EInputMode::EIM_None;
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
	bInvertMinimap = ShouldInvertMinimap();
	if (bInvertMinimap)
	{
		ScaleFactor.Y *= -1.f;
		ScaleFactor.X *= -1.f;
		DrawPos.Y += MapSize;
		DrawPos.X += MapSize;
		MapToScreen = FTranslationMatrix(FVector(DrawPos, 0.0f) / ScaleFactor) * FScaleMatrix(ScaleFactor);
	}
	if (MinimapTexture && Canvas)
	{
		Canvas->DrawColor = DrawColor;
		if (bInvertMinimap)
		{
			Canvas->DrawTile(MinimapTexture, MapToScreen.GetOrigin().X - MapSize, MapToScreen.GetOrigin().Y - MapSize, MapSize, MapSize, 0.0f, MinimapTexture->GetSurfaceHeight(), -1.f * MinimapTexture->GetSurfaceWidth(), -1.f *MinimapTexture->GetSurfaceHeight());
		}
		else
		{
			Canvas->DrawTile(MinimapTexture, MapToScreen.GetOrigin().X, MapToScreen.GetOrigin().Y, MapSize, MapSize, 0.0f, 0.0f, MinimapTexture->GetSurfaceWidth(), MinimapTexture->GetSurfaceHeight());
		}
		DrawMinimapSpectatorIcons();
	}
}

bool AUTHUD::ShouldInvertMinimap()
{
	return false;
}

void AUTHUD::DrawMinimapSpectatorIcons()
{
	if (Canvas == nullptr)
	{
		return;
	}
	const float RenderScale = float(Canvas->SizeY) / 1080.0f;

	// draw pickup icons
	AUTPickup* NamedPickup = NULL;
	FVector2D NamedPickupPos = FVector2D::ZeroVector;
	for (TActorIterator<AUTPickup> It(GetWorld()); It; ++It)
	{
		FCanvasIcon Icon = It->GetMinimapIcon();
		if (Icon.Texture != NULL)
		{
			FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
			const float Ratio = Icon.UL / Icon.VL;
			FLinearColor MutedColor = (LastHoveredActor == *It) ? It->IconColor: It->IconColor * MiniMapIconMuting;
			MutedColor.A = (LastHoveredActor == *It) ? 1.f : MiniMapIconAlpha;
			float IconSize = (LastHoveredActor == *It) ? (48.0f * RenderScale * FMath::InterpEaseOut<float>(1.0f, 1.25f, FMath::Min<float>(0.2f, GetWorld()->RealTimeSeconds - LastHoveredActorChangeTime) * 5.0f, 2.0f)) : (32.0f * RenderScale);
			if (It->FlashOnMinimap())
			{
				float Speed = 2.f;
				float ScaleTime = Speed*GetWorld()->GetTimeSeconds() - int32(Speed*GetWorld()->GetTimeSeconds());
				float Scaling = (ScaleTime < 0.5f)
					? ScaleTime
					: 1.f - ScaleTime;
				MutedColor = It->IconColor * (0.5f + Scaling);
				IconSize = IconSize * (1.f + Scaling);
			}
			Canvas->DrawColor = MutedColor.ToFColor(false);
			Canvas->DrawTile(Icon.Texture, Pos.X - 0.5f * Ratio * IconSize, Pos.Y - 0.5f * IconSize, Ratio * IconSize, IconSize, Icon.U, Icon.V, Icon.UL, Icon.VL);
			if (LastHoveredActor == *It)
			{
				NamedPickup = *It;
				NamedPickupPos = Pos;
			}
		}
	}

	// draw Rally Points  FIXMESTEVE move to FlagRun HUD
	if (UTPlayerOwner && UTPlayerOwner->UTPlayerState && (UTPlayerOwner->UTPlayerState->Team || UTPlayerOwner->UTPlayerState->bOnlySpectator))
	{
		AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
		for (TActorIterator<AUTRallyPoint> It(GetWorld()); It; ++It)
		{
			AUTRallyPoint* RP = *It;
			if (RP)
			{
				FVector2D Pos(WorldToMapToScreen(RP->GetActorLocation()));
				FLinearColor RallyColor = FLinearColor::Gray;
				float IconSizeX = 48.f*RenderScale;
				float IconSizeY = 36.f*RenderScale;
				if (GS && (GS->CurrentRallyPoint == RP) && (UTPlayerOwner->UTPlayerState->bOnlySpectator || GS->bEnemyRallyPointIdentified || (GS->bRedToCap == (UTPlayerOwner->UTPlayerState->Team->TeamIndex == 0))))
				{
					RallyColor = GS->bRedToCap ? FLinearColor::Red : FLinearColor::Blue;
					float Speed = 2.f;
					float ScaleTime = Speed*GetWorld()->GetTimeSeconds() - int32(Speed*GetWorld()->GetTimeSeconds());
					float Scaling = (ScaleTime < 0.5f)
						? ScaleTime
						: 1.f - ScaleTime;
					RallyColor = RallyColor * (0.5f + Scaling);
					IconSizeX = IconSizeX * (1.f + Scaling);
					IconSizeY = IconSizeY * (1.f + Scaling);
				}
				Canvas->DrawColor = RallyColor.ToFColor(false);
				Canvas->DrawTile(HUDAtlas, Pos.X - 0.5f * IconSizeX, Pos.Y - 0.5f * IconSizeY, IconSizeX, IconSizeY, 832.f, 0.f, 64.f, 48.f);
			}
		}
	}

	// draw named areas
	for (TActorIterator<AUTGameVolume> It(GetWorld()); It; ++It)
	{
		AUTGameVolume* GV = *It;
		if (GV && !GV->VolumeName.IsEmpty() && GV->bShowOnMinimap)
		{
			FVector2D Pos(WorldToMapToScreen(GV->GetActorLocation()));
			Pos.X = bInvertMinimap ? Pos.X + GV->MinimapOffset.X * Canvas->ClipX/1920.f : Pos.X - GV->MinimapOffset.X * Canvas->ClipX / 1920.f;
			Pos.Y = bInvertMinimap ? Pos.Y + GV->MinimapOffset.Y * Canvas->ClipX /1280.f : Pos.Y - GV->MinimapOffset.Y * Canvas->ClipX / 1280.f;
			float XL, YL;
			Canvas->TextSize(TinyFont, GV->VolumeName.ToString(), XL, YL);
			Canvas->DrawColor = FColor(0, 0, 0, 64);
			Canvas->DrawTile(SpawnHelpTextBG.Texture, Pos.X - XL * 0.5f, Pos.Y - 0.29f*YL, XL, 0.8f*YL, 149, 138, 32, 32, BLEND_Translucent);
			Canvas->DrawColor = FColor::White;
			Canvas->DrawText(TinyFont, GV->VolumeName, Pos.X - XL * 0.5f, Pos.Y - 0.5f*YL);
		}
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && (!GS->HasMatchStarted() || (GS->IsMatchInProgress() && !GS->IsMatchIntermission())))
	{
		const FVector2D PlayerIconScale = 32.f*RenderScale*FVector2D(1.f, 1.f);
		bool bOnlyShowTeammates = !UTPlayerOwner || !UTPlayerOwner->UTPlayerState || !UTPlayerOwner->UTPlayerState->bOnlySpectator;
		for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(*Iterator);
			if (UTChar)
			{
				// draw team colored dot at location
				AUTPlayerState* PS = Cast<AUTPlayerState>(UTChar->PlayerState);
				if (!PS || !UTPlayerOwner->UTPlayerState || (bOnlyShowTeammates && !PS->bOnlySpectator && (PS != UTPlayerOwner->UTPlayerState) && (!PS->Team || (PS->Team != UTPlayerOwner->UTPlayerState->Team)) && !PS->bSpecialPlayer))
				{
					continue;
				}
				FVector2D Pos(WorldToMapToScreen(UTChar->GetActorLocation()));
				if (bShowScores || bForceScores || bShowScoresWhileDead)
				{
					// draw line from hud to this loc - can't used Canvas line drawing code because it doesn't support translucency
					FVector LineStartPoint(Pos.X, Pos.Y, 0.f);
					FLinearColor LineColor = (PS == GetScorerPlayerState()) ? FLinearColor::Yellow : FLinearColor::White;
					LineColor.A = 0.1f;
					FBatchedElements* BatchedElements = Canvas->Canvas->GetBatchedElements(FCanvas::ET_Line);
					FHitProxyId HitProxyId = Canvas->Canvas->GetHitProxyId();
					BatchedElements->AddTranslucentLine(PS->ScoreCorner, LineStartPoint, LineColor, HitProxyId, 4.f);
				}

				FLinearColor PlayerColor = (PS && PS->Team) ? PS->Team->TeamColor : FLinearColor::Green;
				PlayerColor.A = 1.f;
				float IconRotation = bInvertMinimap ? UTChar->GetActorRotation().Yaw - 90.0f : UTChar->GetActorRotation().Yaw + 90.0f;
				Canvas->K2_DrawTexture(PlayerMinimapTexture, Pos - 0.5f*PlayerIconScale, PlayerIconScale, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), PlayerColor, BLEND_Translucent, IconRotation);

				if (Cast<AUTPlayerController>(PlayerOwner) && (Cast<AUTPlayerController>(PlayerOwner)->LastSpectatedPlayerId == PS->SpectatingID))
				{
					Canvas->DrawColor = FColor(255, 255, 0, 255);
					Canvas->DrawTile(SelectedPlayerTexture, Pos.X - 0.6f*PlayerIconScale.X, Pos.Y - 0.6f*PlayerIconScale.Y, 1.2f*PlayerIconScale.X, 1.2f*PlayerIconScale.Y, 0.0f, 0.0f, SelectedPlayerTexture->GetSurfaceWidth(), SelectedPlayerTexture->GetSurfaceHeight());
				}
			}
		}
		for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(GS->PlayerArray[i]);
			if (PlayerState && !PlayerState->bOnlySpectator && !PlayerState->bPawnWasPostRendered && !PlayerState->LastPostRenderedLocation.IsZero())
			{
				FLinearColor SkullColor = PlayerState->Team ? PlayerState->Team->TeamColor : FLinearColor::White;
				FVector2D Pos(WorldToMapToScreen(PlayerState->LastPostRenderedLocation));
				DrawMinimapIcon(HUDAtlas, Pos, FVector2D(24.f, 24.f) * RenderScale, FVector2D(725.f, 0.f), FVector2D(28.f, 36.f), SkullColor, true);
			}
		}

	}

	// draw name last so it is on top of any conflicting icons
	if (NamedPickup != NULL)
	{
		float XL, YL;
		Canvas->DrawColor = NamedPickup->IconColor.ToFColor(false);
		Canvas->TextSize(TinyFont, NamedPickup->GetDisplayName().ToString(), XL, YL);
		FColor TextColor = Canvas->DrawColor;
		Canvas->DrawColor = FColor(0, 0, 0, 64);
		Canvas->DrawTile(SpawnHelpTextBG.Texture, NamedPickupPos.X - XL * 0.5f, NamedPickupPos.Y - 26.0f * RenderScale - 0.8f*YL, XL, 0.8f*YL, 149, 138, 32, 32, BLEND_Translucent);
		Canvas->DrawColor = TextColor;
		Canvas->DrawText(TinyFont, NamedPickup->GetDisplayName(), NamedPickupPos.X - XL * 0.5f, NamedPickupPos.Y - 26.0f * RenderScale - YL);
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

void AUTHUD::NotifyKill(APlayerState* POVPS, APlayerState* KillerPS, APlayerState* VictimPS)
{
	if (POVPS == KillerPS)
	{
		LastKillTime = GetWorld()->GetTimeSeconds();
		if (GetWorldTimerManager().IsTimerActive(PlayKillHandle))
		{
			PlayKillNotification();
		}
		GetWorldTimerManager().SetTimer(PlayKillHandle, this, &AUTHUD::PlayKillNotification, 0.35f, false);
	}
}

void AUTHUD::PlayKillNotification()
{
	if (GetPlayKillSoundMsg() && UTPlayerOwner)
	{
		UTPlayerOwner->UTClientPlaySound(KillSound);
	}
}

// NOTE: Defaults are defined here because we don't currently have a local profile.
float AUTHUD::GetHUDWidgetOpacity()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetOpacity : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetOpacity;
}

float AUTHUD::GetHUDWidgetBorderOpacity()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetBorderOpacity : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetBorderOpacity;
}

float AUTHUD::GetHUDWidgetSlateOpacity()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetSlateOpacity: UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetSlateOpacity;
}

float AUTHUD::GetHUDWidgetWeaponbarInactiveOpacity()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetWeaponbarInactiveOpacity : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetWeaponbarInactiveOpacity;
}

float AUTHUD::GetHUDWidgetWeaponBarScaleOverride()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetWeaponBarScaleOverride : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetWeaponBarScaleOverride;
}

float AUTHUD::GetHUDWidgetWeaponBarInactiveIconOpacity()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetWeaponBarInactiveIconOpacity : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetWeaponBarInactiveIconOpacity;
}

float AUTHUD::GetHUDWidgetWeaponBarEmptyOpacity()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetWeaponBarEmptyOpacity : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetWeaponBarEmptyOpacity;
}

float AUTHUD::GetHUDWidgetScaleOverride()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDWidgetScaleOverride : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDWidgetScaleOverride;
}

float AUTHUD::GetHUDMessageScaleOverride()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HUDMessageScaleOverride : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDMessageScaleOverride;
}

bool AUTHUD::GetUseWeaponColors()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bUseWeaponColors : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bUseWeaponColors;
}

bool AUTHUD::GetDrawChatKillMsg()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bDrawChatKillMsg : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bDrawChatKillMsg;
}

bool AUTHUD::GetDrawCenteredKillMsg()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bDrawCenteredKillMsg : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bDrawCenteredKillMsg;
}

bool AUTHUD::GetDrawHUDKillIconMsg()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bDrawHUDKillIconMsg : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bDrawHUDKillIconMsg;
}

bool AUTHUD::GetPlayKillSoundMsg()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bPlayKillSoundMsg : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bPlayKillSoundMsg;
}

float AUTHUD::GetQuickStatsAngle()
{
	return VerifyProfileSettings() ? CachedProfileSettings->QuickStatsAngle : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->QuickStatsAngle;
}

float AUTHUD::GetQuickStatsDistance()
{
	return FMath::Clamp<float>((VerifyProfileSettings() ? CachedProfileSettings->QuickStatsDistance : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->QuickStatsDistance), 0.05f, 0.55f);
}

float AUTHUD::GetQuickStatScaleOverride()
{
	return VerifyProfileSettings() ? CachedProfileSettings->QuickStatsScaleOverride: UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->QuickStatsScaleOverride;
}

FName AUTHUD::GetQuickStatsType()
{
	return VerifyProfileSettings() ? CachedProfileSettings->QuickStatsType : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->QuickStatsType;
}

float AUTHUD::GetQuickStatsBackgroundAlpha()
{
	return VerifyProfileSettings() ? CachedProfileSettings->QuickStatsBackgroundAlpha: UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->QuickStatsBackgroundAlpha;
}

float AUTHUD::GetQuickStatsForegroundAlpha()
{
	return VerifyProfileSettings() ? CachedProfileSettings->QuickStatsForegroundAlpha : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->QuickStatsForegroundAlpha;
}

bool AUTHUD::GetQuickStatsHidden()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bQuickStatsHidden : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bQuickStatsHidden;
}

bool AUTHUD::GetHealthArcShown()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bHealthArcShown : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bHealthArcShown;
}

float AUTHUD::GetHealthArcRadius()
{
	return VerifyProfileSettings() ? CachedProfileSettings->HealthArcRadius : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HealthArcRadius;
}

bool AUTHUD::GetQuickInfoHidden()
{
	return VerifyProfileSettings() ? CachedProfileSettings->bQuickInfoHidden : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->bQuickInfoHidden;
}

bool AUTHUD::ProcessInputAxis(FKey Key, float Delta)
{
	for (int32 i=0; i < RadialMenus.Num(); i++)
	{
		if (RadialMenus[i] != nullptr)
		{
			if (RadialMenus[i]->ProcessInputAxis(Key, Delta))
			{
				return true;
			}
		}
	}

	return false;
}

bool AUTHUD::ProcessInputKey(FKey Key, EInputEvent EventType)
{
	for (int32 i=0; i < RadialMenus.Num(); i++)
	{
		if (RadialMenus[i] != nullptr)
		{
			if ( RadialMenus[i]->ProcessInputKey(Key, EventType) )
			{
				return true;
			}
		}
	}

	return false;
}

void AUTHUD::ToggleComsMenu(bool bShow)
{
	bShowComsMenu = bShow;

	if (bShow)
	{
		ComsMenu->BecomeInteractive();
	}
	else
	{
		ComsMenu->BecomeNonInteractive();
	}
}

void AUTHUD::ToggleWeaponWheel(bool bShow)
{
	bShowWeaponWheel = bShow;

	AUTCharacter* UTCharacter = UTPlayerOwner ? UTPlayerOwner->GetUTCharacter() : nullptr;
	if (bShow && UTCharacter && !UTCharacter->IsDead())
	{
		WeaponWheel->BecomeInteractive();
	}
	else
	{
		WeaponWheel->BecomeNonInteractive();
	}
}

TWeakObjectPtr<class UUTUMGHudWidget> AUTHUD::ActivateUMGHudWidget(FString UMGHudWidgetClassName, bool bUnique)
{
	TWeakObjectPtr<class UUTUMGHudWidget> FinalUMGWidget;
	FinalUMGWidget.Reset();

	if ( !UMGHudWidgetClassName.IsEmpty() ) 
	{

		// Attempt to look up the class
		UClass* UMGWidgetClass = LoadClass<UUTUMGHudWidget>(NULL, *UMGHudWidgetClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
		if (UMGWidgetClass)
		{
			// Look to see if there is a widget in the stack that matches this class.  And if so then exit.
			if (bUnique)
			{
				for (int32 i=0; i < UMGHudWidgetStack.Num(); i++)
				{
					if (UMGHudWidgetStack[i].IsValid() && UMGHudWidgetStack[i]->GetClass() == UMGWidgetClass)
					{
						return FinalUMGWidget;
					}
				}
			}

			FinalUMGWidget = CreateWidget<UUTUMGHudWidget>(UTPlayerOwner, UMGWidgetClass);
			if (FinalUMGWidget.IsValid())
			{
				ActivateActualUMGHudWidget(FinalUMGWidget);
			}
		}		
		else	// The class wasn't found, so try again but this time add a _C to the classname
		{
			return ActivateUMGHudWidget(UMGHudWidgetClassName + TEXT("_C"), bUnique);
		}
	}
	return FinalUMGWidget;
}

bool AUTHUD::IsUMGWidgetActive(TWeakObjectPtr<UUTUMGHudWidget> TestWidget)
{
	for (int i = 0; i < UMGHudWidgetStack.Num(); i++)
	{
		if (UMGHudWidgetStack[i].Get() == TestWidget.Get())
		{
			return true;
		}
	}

	return false;
}

void AUTHUD::ActivateActualUMGHudWidget(TWeakObjectPtr<UUTUMGHudWidget> WidgetToActivate)
{
	UMGHudWidgetStack.Add(WidgetToActivate);
	WidgetToActivate->AssociateHUD(this);

	// UMG annoying adds +10 to the zorder (hard coded).  I could just tell the artists to start their DisplayZOrder usage at -11 or something 
	// similar, but then I'd have to worry about them.  So I just subtract 10,000 from the DisplayZOrder.  
	WidgetToActivate->AddToViewport(WidgetToActivate->DisplayZOrder - 10000.0f);
}

void AUTHUD::DeactivateUMGHudWidget(FString UMGHudWidgetClassName)
{
	// Attempt to look up the class
	UClass* UMGWidgetClass = LoadClass<UUserWidget>(NULL, *UMGHudWidgetClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (UMGWidgetClass && UMGWidgetClass->IsChildOf(UUTUMGHudWidget::StaticClass()))
	{
		// Look to see if there is a widget in the stack that matches this class.  And if so then exit.
		for (int32 i=0; i < UMGHudWidgetStack.Num(); i++)
		{
			if (UMGHudWidgetStack[i].IsValid() && UMGHudWidgetStack[i]->GetClass() == UMGWidgetClass)
			{
				DeactivateActualUMGHudWidget(UMGHudWidgetStack[i]);
			}
		}
	}
}

void AUTHUD::DeactivateActualUMGHudWidget(TWeakObjectPtr<UUTUMGHudWidget> WidgetToDeactivate)
{
	WidgetToDeactivate->RemoveFromViewport();
	UMGHudWidgetStack.Remove(WidgetToDeactivate);
}

UUTCrosshair* AUTHUD::GetCrosshairForWeapon(FName WeaponCustomizationTag, FWeaponCustomizationInfo& outWeaponCustomizationInfo)
{
	if ( VerifyProfileSettings() )
	{
		CachedProfileSettings->GetWeaponCustomization(WeaponCustomizationTag, outWeaponCustomizationInfo);	
	}
	else
	{
		outWeaponCustomizationInfo = FWeaponCustomizationInfo();
	}

	FName DesiredCrosshairName = outWeaponCustomizationInfo.CrosshairTag;
	if (DesiredCrosshairName == NAME_None) DesiredCrosshairName = FName(TEXT("CrossDot"));
	if (Crosshairs.Contains(DesiredCrosshairName))
	{
		return Crosshairs[DesiredCrosshairName];
	}

	return nullptr;
}

float AUTHUD::DrawWinConditions(UFont* InFont, float XPos, float YPos, float ScoreWidth, float RenderScale, bool bCenterMessage, bool bSkipDrawing)
{
	if (!bSkipDrawing)
	{
		DrawText(ScoreMessageText.ToString(), FLinearColor::White, XPos, YPos, InFont, RenderScale, 1.f);
	}
	return 0.f;
}


