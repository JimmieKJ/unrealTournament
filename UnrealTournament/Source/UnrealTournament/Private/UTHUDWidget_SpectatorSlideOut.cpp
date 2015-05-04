// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTPlayerInput.h"
#include "UTCTFGameState.h"
#include "UTSpectatorCamera.h"
#include "UTPickupInventory.h"

UUTHUDWidget_SpectatorSlideOut::UUTHUDWidget_SpectatorSlideOut(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(500.0f, 108.0f);
	ScreenPosition = FVector2D(0.0f, 0.005f);
	Origin = FVector2D(0.0f, 0.0f);

	FlagX = 0.09;
	ColumnHeaderPlayerX = 0.16;
	ColumnHeaderScoreX = 0.7;
	ColumnHeaderArmor = 0.87f;
	ColumnY = 12; // @TODO FIXMESTEVE why not percent of canvas size

	CellHeight = 40;
	SlideIn = 0.f;
	CenterBuffer = 10.f;
	SlideSpeed = 6.f;
	ActionHighlightTime = 1.1f;

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FlagTex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/CountryFlags.CountryFlags'"));
	FlagAtlas = FlagTex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture> UDamTex(TEXT("Texture'/Game/RestrictedAssets/UI/HUDAtlas01.HUDAtlas01'"));
	UDamageHUDIcon.Texture = UDamTex.Object;
	UDamageHUDIcon.U = 589.f;
	UDamageHUDIcon.V = 0.f;
	UDamageHUDIcon.UL = 45.f;
	UDamageHUDIcon.VL = 39.f;

	HealthIcon.Texture = UDamTex.Object;
	HealthIcon.U = 522.f;
	HealthIcon.V = 0.f;
	HealthIcon.UL = 37.f;
	HealthIcon.VL = 35.f;

	ArmorIcon.Texture = UDamTex.Object;
	ArmorIcon.U = 560.f;
	ArmorIcon.V = 0.f;
	ArmorIcon.UL = 28.f;
	ArmorIcon.VL = 36.f;

	static ConstructorHelpers::FObjectFinder<UTexture> CTFTex(TEXT("Texture'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	FlagIcon.Texture = CTFTex.Object;
	FlagIcon.U = 843.f;
	FlagIcon.V = 87.f;
	FlagIcon.UL = 43.f;
	FlagIcon.VL = 41.f;

	RedFlagBind = NAME_None;
	BlueFlagBind = NAME_None;
}

void UUTHUDWidget_SpectatorSlideOut::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	// cache spectating keybinds
	UTPlayerOwner = Hud ? Hud->UTPlayerOwner : NULL;
	if (UTPlayerOwner != NULL)
	{
		UUTPlayerInput* Input = Cast<UUTPlayerInput>(UTPlayerOwner->PlayerInput);
		if (Input)
		{
			int32 CameraIndex = 0;
			for (int32 i = 0; i < Input->SpectatorBinds.Num(); i++)
			{
				if (Input->SpectatorBinds[i].Command == "ViewFlag 0")
				{
					RedFlagBind = Input->SpectatorBinds[i].KeyName;
				}
				else if (Input->SpectatorBinds[i].Command == "ViewFlag 1")
				{
					BlueFlagBind = Input->SpectatorBinds[i].KeyName;
				}
				else if (Input->SpectatorBinds[i].Command.Left(10) == "ViewCamera")
				{
					// @TODO FIXMESTEVE - parse and get value
					CameraBind[CameraIndex] = Input->SpectatorBinds[i].KeyName;
					CameraIndex++;
				}
			}
		}
	}
}

bool UUTHUDWidget_SpectatorSlideOut::ShouldDraw_Implementation(bool bShowScores)
{
	if (!bShowScores && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTGameState && UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
	{
		if (UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted() || UTGameState->IsMatchAtHalftime())
		{
			return false;
		}
		return (UTHUDOwner->UTPlayerOwner->bRequestingSlideOut || UTHUDOwner->UTPlayerOwner->bShowCameraBinds || (SlideIn > 0.f));
	}
	return false;
}

void UUTHUDWidget_SpectatorSlideOut::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	if (TextureAtlas && UTGameState)
	{
		SlideIn = (UTHUDOwner->UTPlayerOwner->bRequestingSlideOut || UTHUDOwner->UTPlayerOwner->bShowCameraBinds) 
					? FMath::Min(Size.X, SlideIn + DeltaTime*Size.X*SlideSpeed) 
					: FMath::Max(0.f, SlideIn - DeltaTime*Size.X*SlideSpeed);

		int32 MaxRedPlaces = UTGameState->bTeamGame ? 5 : 10; // warning: hardcoded to match default binds, which has 5 red and 5 blue
		int32 XOffset = SlideIn - Size.X;
		float DrawOffset = 0.f;

		if (UTHUDOwner->UTPlayerOwner->bShowCameraBinds)
		{
			DrawCamBind(NAME_None, "Press number to view player", DeltaTime, XOffset, DrawOffset, false);
		}
		else
		{
			DrawOffset += CellHeight;
		}
		DrawOffset += CellHeight;

		TArray<AUTPlayerState*> RedPlayerList, BluePlayerList;
		for (APlayerState* PS : UTGameState->PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL && !UTPS->bOnlySpectator)
			{
				if (!UTGameState->bTeamGame)
				{
					RedPlayerList.Add(UTPS);
				}
				else if (UTPS->GetTeamNum() == 0)
				{
					RedPlayerList.Add(UTPS);
				}
				else if (UTPS->GetTeamNum() == 1)
				{
					BluePlayerList.Add(UTPS);
				}
			}
		}
		bool(*SortFunc)(const AUTPlayerState&, const AUTPlayerState&);
		if (UTGameState->bTeamGame)
		{
			SortFunc = [](const AUTPlayerState& A, const AUTPlayerState& B)
			{
				return A.SpectatingIDTeam < B.SpectatingIDTeam;
			};
		}
		else
		{
			SortFunc = [](const AUTPlayerState& A, const AUTPlayerState& B)
			{
				return A.SpectatingID < B.SpectatingID;
			};
		}
		RedPlayerList.Sort(SortFunc);
		for (int32 PlayerBind = 1; PlayerBind <= MaxRedPlaces; PlayerBind++)
		{
			for (int32 i = 0; i < RedPlayerList.Num(); i++)
			{
				AUTPlayerState* PlayerState = RedPlayerList[i];
				uint8 Place = UTGameState->bTeamGame ? PlayerState->SpectatingIDTeam : PlayerState->SpectatingID;
				if ((Place % MaxRedPlaces) == (PlayerBind % MaxRedPlaces))
				{
					DrawPlayer(PlayerBind, PlayerState, DeltaTime, XOffset, DrawOffset);
					DrawOffset += CellHeight;
					break;
				}
			}
		}
		if (UTGameState->bTeamGame)
		{
			if (!UTHUDOwner->UTPlayerOwner->bShowCameraBinds)
			{
				DrawOffset += CellHeight;
				DrawOffset += CellHeight;
			}
			BluePlayerList.Sort(SortFunc);
			for (int32 PlayerBind = 1; PlayerBind <= MaxRedPlaces; PlayerBind++)
			{
				for (int32 i = 0; i < BluePlayerList.Num(); i++)
				{
					AUTPlayerState* PlayerState = BluePlayerList[i];
					uint8 Place = PlayerState->SpectatingIDTeam;
					if ((Place % MaxRedPlaces) == (PlayerBind % MaxRedPlaces))
					{
						DrawPlayer(MaxRedPlaces + PlayerBind, PlayerState, DeltaTime, XOffset, DrawOffset);
						DrawOffset += CellHeight;
						break;
					}
				}
			}
		}

		if (!UTHUDOwner->UTPlayerOwner->bShowCameraBinds)
		{
			//draw powerup clocks
			for (FActorIterator It(GetWorld()); It; ++It)
			{
				AUTPickupInventory* Pickup = Cast<AUTPickupInventory>(*It);
				if (Pickup && Pickup->GetInventoryType() && Pickup->GetInventoryType()->GetDefaultObject<AUTInventory>()->bShowPowerupTimer && (Pickup->GetInventoryType()->GetDefaultObject<AUTInventory>()->HUDIcon.Texture != NULL))
				{
					FLinearColor BarColor = FLinearColor::White;
					float FinalBarOpacity = 0.3f;
					DrawTexture(TextureAtlas, XOffset, DrawOffset, 0.3f*Size.X, 36, 149, 138, 32, 32, FinalBarOpacity, FLinearColor::White);

					FCanvasIcon HUDIcon = Pickup->GetInventoryType()->GetDefaultObject<AUTInventory>()->HUDIcon;
					DrawTexture(HUDIcon.Texture, XOffset + 0.1f*Canvas->ClipX + 4.f, DrawOffset, 0.1f*Size.X, 0.1f*Size.X, HUDIcon.U, HUDIcon.V, HUDIcon.UL, HUDIcon.VL, 1.f, FLinearColor::White, FVector2D(1.0, 0.0));

					FFormatNamedArguments Args;
					Args.Add("TimeRemaining", FText::AsNumber(int32(GetWorld()->GetTimerManager().GetTimerRemaining(Pickup->WakeUpTimerHandle))));
					FLinearColor DrawColor = FLinearColor::White;
					DrawColor.R *= 0.5f;
					DrawColor.G *= 0.5f;
					DrawColor.B *= 0.5f;
					DrawText(FText::Format(NSLOCTEXT("UTCharacter", "PowerupTimeDisplay", "{TimeRemaining}"), Args), XOffset + 0.15f*Size.X, DrawOffset+ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
					DrawOffset += 1.2f*CellHeight;
				}

				// also armor, superhealth

			}
		}
		AUTCTFGameState * CTFGameState = Cast<AUTCTFGameState>(UTGameState);
		UUTPlayerInput* Input = Cast<UUTPlayerInput>(UTHUDOwner->PlayerOwner->PlayerInput);
		if (Input && UTHUDOwner->UTPlayerOwner->bShowCameraBinds)
		{
			if (!bCamerasInitialized)
			{
				bCamerasInitialized = true;
				NumCameras = 0;
				for (FActorIterator It(GetWorld()); It; ++It)
				{
					AUTSpectatorCamera* Cam = Cast<AUTSpectatorCamera>(*It);
					if (Cam)
					{
						CameraString[NumCameras] = Cam->CamLocationName;
						NumCameras++;
						if (NumCameras == 10)
						{
							break;
						}
					}
				}
			}
			DrawOffset += 0.25f*(10.f-float(NumCameras))*CellHeight;
			if (CTFGameState && (CTFGameState->FlagBases.Num() > 1))
			{
				// show flag binds
				if ((RedFlagBind != NAME_None) && CTFGameState->FlagBases[0] && CTFGameState->FlagBases[0]->MyFlag)
				{
					DrawFlag(RedFlagBind, "Red Flag", CTFGameState->FlagBases[0]->MyFlag, DeltaTime, XOffset, DrawOffset);
					DrawOffset += CellHeight;
				}
				if ((BlueFlagBind != NAME_None) && CTFGameState->FlagBases[1] && CTFGameState->FlagBases[1]->MyFlag)
				{
					DrawFlag(BlueFlagBind, "Blue Flag", CTFGameState->FlagBases[1]->MyFlag, DeltaTime, XOffset, DrawOffset);
					DrawOffset += CellHeight;
				}
			}
			for (int32 i = 0; i < Input->SpectatorBinds.Num(); i++)
			{
				if ((Input->SpectatorBinds[i].FriendlyName != "") && (Input->SpectatorBinds[i].KeyName != NAME_None))
				{
					DrawCamBind(Input->SpectatorBinds[i].KeyName, Input->SpectatorBinds[i].FriendlyName, DeltaTime, XOffset, DrawOffset, (Cast<AUTProjectile>(UTHUDOwner->UTPlayerOwner->GetViewTarget()) != NULL));
					DrawOffset += CellHeight;
				}
			}
			static FName NAME_PressFire = FName(TEXT("Fire"));
			DrawCamBind(NAME_PressFire, "View Next Player", DeltaTime, XOffset, DrawOffset, false);
			DrawOffset += CellHeight;
			static FName NAME_PressAltFire = FName(TEXT("AltFire"));
			DrawCamBind(NAME_PressAltFire, "Free Cam", DeltaTime, XOffset, DrawOffset, false);
			DrawOffset += CellHeight;

			bool bOverflow = false;
			for (int32 i = 0; i < NumCameras; i++)
			{
				if (CameraBind[i] != NAME_None)
				{
					DrawCamBind(CameraBind[i], CameraString[i], DeltaTime, XOffset, DrawOffset, false);
					if (!bOverflow)
					{
						DrawOffset += CellHeight;
						if (DrawOffset > Canvas->ClipY - CellHeight)
						{
							bOverflow = true;
							XOffset = XOffset + Size.X + 2.f;
							DrawOffset -= CellHeight;
						}
					}
					else
					{
						DrawOffset -= CellHeight;
					}
				}
			}
		}
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawFlag(FName KeyName, FString FlagName, AUTCarriedObject* Flag, float RenderDelta, float XOffset, float YOffset)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = Size.X;

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::White;
	float FinalBarOpacity = BarOpacity;
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 36, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	if (Flag == UTHUDOwner->UTPlayerOwner->GetViewTarget())
	{
		DrawTexture(TextureAtlas, XOffset + Width, YOffset, 35, 36, 36, 188, -36, 65, FinalBarOpacity, BarColor);
	}

	// Draw the Text
	DrawText(FText::FromString("[" + KeyName.ToString() + "]"), XOffset + 4.f, YOffset + ColumnY, UTHUDOwner->MediumFont, 0.5f, 0.5f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	FLinearColor FlagColor = Flag->Team ? Flag->Team->TeamColor : FLinearColor::White;
	DrawTexture(FlagIcon.Texture, XOffset + (Width * 0.5f), YOffset + ColumnY - 0.025f*Width, 0.09f*Width, 0.09f*Width, FlagIcon.U, FlagIcon.V, FlagIcon.UL, FlagIcon.VL, 1.0, FlagColor, FVector2D(1.0, 0.0));

	DrawText(FText::FromString(FlagName), XOffset + (Width * 0.6f), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, FlagColor, ETextHorzPos::Left, ETextVertPos::Center);
}

void UUTHUDWidget_SpectatorSlideOut::DrawCamBind(FName KeyName, FString ProjName, float RenderDelta, float XOffset, float YOffset, bool bCamSelected)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = Size.X;

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::White;
	float FinalBarOpacity = BarOpacity;
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 36, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	if (bCamSelected)
	{
		DrawTexture(TextureAtlas, XOffset + Width, YOffset, 35, 36, 36, 188, -36, 65, FinalBarOpacity, BarColor);
	}

	// Draw the Text
	if (KeyName != NAME_None)
	{
		DrawText(FText::FromString("[" + KeyName.ToString() + "]"), XOffset + 4.f, YOffset + ColumnY, UTHUDOwner->MediumFont, 0.5f, 0.5f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
	}
	DrawText(FText::FromString(ProjName), XOffset + (Width * 0.98f), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Right, ETextVertPos::Center);
}

void UUTHUDWidget_SpectatorSlideOut::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	if (PlayerState == NULL) return;

	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = Size.X;

	FText Position = FText::Format(NSLOCTEXT("UTScoreboard", "PositionFormatText", "{0}."), FText::AsNumber(Index));
	FText PlayerName = FText::FromString(GetClampedName(PlayerState, UTHUDOwner->MediumFont, 1.f, 0.475f*Width));
	FText PlayerScore = FText::AsNumber(int32(PlayerState->Score));

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::Black;
	if (PlayerState->Team)
	{
		BarColor = PlayerState->Team->TeamColor;
	}
	float FinalBarOpacity = BarOpacity;

	AUTCharacter* Character = PlayerState->GetUTCharacter();
	if (Character && (Character->Health > 0))
	{
		float LastActionTime = GetWorld()->GetTimeSeconds() - FMath::Max(Character->LastTakeHitTime, Character->LastWeaponFireTime);

		if (LastActionTime < ActionHighlightTime)
		{
			float Blend = 1.f - LastActionTime / ActionHighlightTime;
			BarColor.R = BarColor.R + (1.f - BarColor.R)*Blend;
			BarColor.G = BarColor.G + (1.f - BarColor.G)*Blend;
			BarColor.B = BarColor.B + (1.f - BarColor.B)*Blend;
			FinalBarOpacity = 0.75f;
		}
	}
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 36, 149, 138, 32, 32, FinalBarOpacity, BarColor);	

	if ((PlayerState == UTHUDOwner->UTPlayerOwner->LastSpectatedPlayerState) || (PlayerState->CarriedObject && (PlayerState->CarriedObject == UTHUDOwner->UTPlayerOwner->GetViewTarget())))
	{
		DrawTexture(TextureAtlas, XOffset + Width, YOffset, 35, 36, 36, 188, -36, 65, FinalBarOpacity, BarColor);
	}

	int32 FlagU = (PlayerState->CountryFlag % 8) * 32;
	int32 FlagV = (PlayerState->CountryFlag / 8) * 24;

	DrawTexture(FlagAtlas, XOffset + (Width * FlagX), YOffset + 18, 32, 24, FlagU, FlagV, 32, 24, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));	

	// Draw the Text
	DrawText(Position, XOffset + 4.f, YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
	FVector2D NameSize = DrawText(PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	if (UTGameState && UTGameState->HasMatchStarted() && Character)
	{
		if (PlayerState->CarriedObject)
		{
			FLinearColor FlagColor = PlayerState->CarriedObject->Team ? PlayerState->CarriedObject->Team->TeamColor : FLinearColor::White;
			DrawTexture(FlagIcon.Texture, XOffset + (Width * (ColumnHeaderScoreX - 0.1f)), YOffset + ColumnY - 0.025f*Width, 0.09f*Width, 0.09f*Width, FlagIcon.U, FlagIcon.V, FlagIcon.UL, FlagIcon.VL, 1.0, FlagColor, FVector2D(1.0, 0.0));
		}
		if (Character->GetWeaponOverlayFlags() != 0)
		{
			// @TODO FIXMESTEVE - support actual overlays for different powerups - save related class when setting up overlays in GameState, so have easy mapping. 
			// For now just assume UDamage
			DrawTexture(UDamageHUDIcon.Texture, XOffset + (Width * (ColumnHeaderScoreX - 0.1f)), YOffset + ColumnY - 0.05f*Width, 0.1f*Width, 0.1f*Width, UDamageHUDIcon.U, UDamageHUDIcon.V, UDamageHUDIcon.UL, UDamageHUDIcon.VL, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
		}

		DrawTexture(HealthIcon.Texture, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY - 0.015f*Width, 0.05f*Width, 0.05f*Width, HealthIcon.U, HealthIcon.V, HealthIcon.UL, HealthIcon.VL, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
		FFormatNamedArguments Args;
		Args.Add("Health", FText::AsNumber(Character->Health));
		DrawColor = FLinearColor::Green;
		DrawColor.R *= 0.5f;
		DrawColor.G *= 0.5f;
		DrawColor.B *= 0.5f;
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "HealthDisplay", "{Health}"), Args), XOffset + (Width * (ColumnHeaderScoreX + 0.05f)), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);

		if (Character->ArmorAmount > 0)
		{
			DrawTexture(ArmorIcon.Texture, XOffset + (Width * ColumnHeaderArmor), YOffset + ColumnY - 0.015f*Width, 0.05f*Width, 0.05f*Width, ArmorIcon.U, ArmorIcon.V, ArmorIcon.UL, ArmorIcon.VL, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
			FFormatNamedArguments Args;
			Args.Add("Armor", FText::AsNumber(Character->ArmorAmount));
			DrawColor = FLinearColor::Yellow;
			DrawColor.R *= 0.5f;
			DrawColor.G *= 0.5f;
			DrawColor.B *= 0.5f;
			DrawText(FText::Format(NSLOCTEXT("UTCharacter", "ArmorDisplay", "{Armor}"), Args), XOffset + (Width * (ColumnHeaderArmor + 0.065f)), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		}
	}
}
