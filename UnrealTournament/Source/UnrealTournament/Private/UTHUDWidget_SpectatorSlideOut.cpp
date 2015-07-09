// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTPlayerInput.h"
#include "UTCTFGameState.h"
#include "UTSpectatorCamera.h"
#include "UTPickupInventory.h"
#include "UTArmor.h"
#include "UTDemoRecSpectator.h"

UUTHUDWidget_SpectatorSlideOut::UUTHUDWidget_SpectatorSlideOut(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(360.0f, 108.0f);
	ScreenPosition = FVector2D(0.0f, 0.005f);
	Origin = FVector2D(0.0f, 0.0f);

	FlagX = 0.09;
	ColumnHeaderPlayerX = 0.18f;
	ColumnHeaderScoreX = 0.7f;
	ColumnHeaderArmor = 0.89f;
	ColumnY = 0.11f * Size.Y;

	CellHeight = 32;
	SlideIn = 0.f;
	CenterBuffer = 10.f;
	SlideSpeed = 6.f;
	ActionHighlightTime = 1.1f;

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FlagTex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/CountryFlags.CountryFlags'"));
	FlagAtlas = FlagTex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> WeaponTex(TEXT("Texture'/Game/RestrictedAssets/UI/WeaponAtlas01.WeaponAtlas01'"));
	WeaponAtlas = WeaponTex.Object;

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
					CameraBind[CameraIndex] = Input->SpectatorBinds[i].KeyName;
					CameraIndex++;
				}
				else if (Input->SpectatorBinds[i].Command == "EnableAutoCam")
				{
					AutoCamBind = Input->SpectatorBinds[i].KeyName;
				}
				else if (Input->SpectatorBinds[i].Command == "DemoRestart")
				{
					DemoRestartBind = Input->SpectatorBinds[i].KeyName;
				}
				else if (Input->SpectatorBinds[i].Command == "DemoGoToLive")
				{
					DemoLiveBind = Input->SpectatorBinds[i].KeyName;
				}
				else if (Input->SpectatorBinds[i].Command == "DemoPause")
				{
					DemoPauseBind = Input->SpectatorBinds[i].KeyName;
				}
				else if (Input->SpectatorBinds[i].Command == "DemoSeek -5")
				{
					DemoRewindBind = Input->SpectatorBinds[i].KeyName;
				}
				else if (Input->SpectatorBinds[i].Command == "DemoSeek 10")
				{
					DemoFastForwardBind = Input->SpectatorBinds[i].KeyName;
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

void UUTHUDWidget_SpectatorSlideOut::InitPowerupList()
{
	bPowerupListInitialized = true;
	if (UTGameState != nullptr)
	{
		UTGameState->GetImportantPickups(PowerupList);
	}
}

void UUTHUDWidget_SpectatorSlideOut::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	if (bIsInteractive)
	{
		ClickElementStack.Empty();
	}

	// Hack to allow animations during pause
	DeltaTime = GetWorld()->DeltaTimeSeconds;

	if (TextureAtlas && UTGameState)
	{
		SlideIn = (UTHUDOwner->UTPlayerOwner->bRequestingSlideOut || UTHUDOwner->UTPlayerOwner->bShowCameraBinds) 
					? FMath::Min(Size.X, SlideIn + DeltaTime*Size.X*SlideSpeed) 
					: FMath::Max(0.f, SlideIn - DeltaTime*Size.X*SlideSpeed);

		int32 MaxRedPlaces = UTGameState->bTeamGame ? 5 : 10; // warning: hardcoded to match default binds, which has 5 red and 5 blue
		float XOffset = SlideIn - Size.X;
		float DrawOffset = 0.27f * Canvas->ClipY;
		SlideOutFont = UTHUDOwner->SmallFont;

		if (UTHUDOwner->UTPlayerOwner->bShowCameraBinds)
		{
			DrawCamBind(NAME_None, "Press number to view player", DeltaTime, XOffset, DrawOffset, false);
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
					PlayerState->bNeedsAssistAnnouncement = true; // hack - use this transient property
					break;
				}
			}
		}
		for (int32 i = 0; i < RedPlayerList.Num(); i++)
		{
			AUTPlayerState* PlayerState = RedPlayerList[i];
			if (!PlayerState->bNeedsAssistAnnouncement)
			{
				DrawPlayer(-1, PlayerState, DeltaTime, XOffset, DrawOffset);
				DrawOffset += CellHeight;
			}
			PlayerState->bNeedsAssistAnnouncement = false;
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
						PlayerState->bNeedsAssistAnnouncement = true;
						break;
					}
				}
			}
			for (int32 i = 0; i < BluePlayerList.Num(); i++)
			{
				AUTPlayerState* PlayerState = BluePlayerList[i];
				if (!PlayerState->bNeedsAssistAnnouncement)
				{
					DrawPlayer(-1, PlayerState, DeltaTime, XOffset, DrawOffset);
					DrawOffset += CellHeight;
				}
				PlayerState->bNeedsAssistAnnouncement = false;
			}
		}

		if (!UTHUDOwner->UTPlayerOwner->bShowCameraBinds)
		{
			if (!bPowerupListInitialized)
			{
				InitPowerupList();
			}
			//draw powerup clocks
			DrawOffset += 0.2f*CellHeight;
			for (int32 i = 0; i < PowerupList.Num(); i++)
			{
				if (PowerupList[i] != NULL)
				{
					DrawPowerup(PowerupList[i], XOffset, DrawOffset);
					DrawOffset += 1.2f*CellHeight;
				}
			}
		}
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
			DrawCamBind(AutoCamBind, "Auto Camera", DeltaTime, XOffset, DrawOffset, false);
			DrawOffset += CellHeight;

			AUTCTFGameState * CTFGameState = Cast<AUTCTFGameState>(UTGameState);
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
			DrawCamBind(NAME_PressFire, "Camera Rotation Control", DeltaTime, XOffset, DrawOffset, false);
			DrawOffset += CellHeight;
			static FName NAME_PressAltFire = FName(TEXT("AltFire"));
			DrawCamBind(NAME_PressAltFire, "Free Cam", DeltaTime, XOffset, DrawOffset, false);
			DrawOffset += CellHeight;

			if (Cast<AUTDemoRecSpectator>(UTHUDOwner->PlayerOwner) != nullptr)
			{
				DrawCamBind(DemoRestartBind, "Restart Demo", DeltaTime, XOffset, DrawOffset, false);
				DrawOffset += CellHeight;

				DrawCamBind(DemoRewindBind, "Rewind Demo", DeltaTime, XOffset, DrawOffset, false);
				DrawOffset += CellHeight;

				DrawCamBind(DemoFastForwardBind, "Fast Forward", DeltaTime, XOffset, DrawOffset, false);
				DrawOffset += CellHeight;

				DrawCamBind(DemoLiveBind, "Jump to Real Time", DeltaTime, XOffset, DrawOffset, false);
				DrawOffset += CellHeight;

				DrawCamBind(DemoPauseBind, "Pause Demo", DeltaTime, XOffset, DrawOffset, false);
				DrawOffset += CellHeight;
			}

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
							XOffset = XOffset + 0.75f * Size.X + 2.f;
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

void UUTHUDWidget_SpectatorSlideOut::DrawPowerup(AUTPickup* Pickup, float XOffset, float YOffset)
{
	// @TODO FIXMESTEVE get rid of armor hacks when they have icons
	FLinearColor BarColor = FLinearColor::White;
	float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(Pickup->WakeUpTimerHandle);
	float FinalBarOpacity = (RemainingTime > 0.f) ? FMath::Clamp(1.f - 0.4f*(Pickup->RespawnTime - RemainingTime), 0.3f, 1.f) : 1.f;
	DrawTexture(TextureAtlas, XOffset, YOffset, 0.4f*Size.X, 0.95f*CellHeight, 149, 138, 32, 32, FinalBarOpacity, FLinearColor::White);

	if (Pickup->TeamSide < 2)
	{
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset, YOffset, 0.08f*Size.X, 0.08f*Size.X, UTHUDOwner->TeamIconUV[Pickup->TeamSide].X, UTHUDOwner->TeamIconUV[Pickup->TeamSide].Y, 72, 72, 1.f, ((Pickup->TeamSide == 0) ? FLinearColor::Red : FLinearColor::Blue));
	}
	AUTPickupInventory* PickupInventory = Cast<AUTPickupInventory>(Pickup);
	if (PickupInventory && PickupInventory->GetInventoryType()->IsChildOf(AUTArmor::StaticClass()))
	{
		DrawTexture(ArmorIcon.Texture, XOffset + 0.12f*Size.X, YOffset, 0.085f*Size.X, 0.085f*Size.X, ArmorIcon.U, ArmorIcon.V, ArmorIcon.UL, ArmorIcon.VL, 1.f, FLinearColor::White);
		FFormatNamedArguments Args;
		Args.Add("Armor", FText::AsNumber(PickupInventory->GetInventoryType()->GetDefaultObject<AUTArmor>()->ArmorAmount));
		FLinearColor DrawColor = FLinearColor::Yellow;
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "ArmorDisplay", "{Armor}"), Args), XOffset + 0.16f*Size.X, YOffset + ColumnY, SlideOutFont, FVector2D(1.f, 1.f), FLinearColor::Black, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	else if (PickupInventory)
	{
		FCanvasIcon HUDIcon = PickupInventory->GetInventoryType()->GetDefaultObject<AUTInventory>()->HUDIcon;
		DrawTexture(HUDIcon.Texture, XOffset + 0.1f*Size.X, YOffset - 0.021f*Size.X, 0.1f*Size.X, 0.1f*Size.X, HUDIcon.U, HUDIcon.V, HUDIcon.UL, HUDIcon.VL, 0.8f, FLinearColor::White);
	}
	else
	{
		FCanvasIcon HUDIcon = Pickup->HUDIcon;
		DrawTexture(HUDIcon.Texture, XOffset + 0.1f*Size.X, YOffset - 0.021f*Size.X, 0.1f*Size.X, 0.1f*Size.X, HUDIcon.U, HUDIcon.V, HUDIcon.UL, HUDIcon.VL, 0.8f, FLinearColor::White);
	}

	FLinearColor DrawColor = FLinearColor::White;
	if (RemainingTime > 0.f)
	{
		FFormatNamedArguments Args;
		Args.Add("TimeRemaining", FText::AsNumber(int32(RemainingTime)));
		DrawColor.R *= 0.5f;
		DrawColor.G *= 0.5f;
		DrawColor.B *= 0.5f;
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "PowerupTimeDisplay", "{TimeRemaining}"), Args), XOffset + 0.28f*Size.X, YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	else
	{
		DrawText(NSLOCTEXT("UTCharacter", "PowerupUp", "UP"), XOffset + 0.28f*Size.X, YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawFlag(FName KeyName, FString FlagName, AUTCarriedObject* Flag, float RenderDelta, float XOffset, float YOffset)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = 0.75f * Size.X;

	// If we are interactive and this element has a keybind, store it so the mouse can click it
	if (bIsInteractive && KeyName != NAME_None)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + Width) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));
		ClickElementStack.Add(FClickElement(KeyName, Bounds));
		if (MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W)
		{
			BarOpacity = 0.5;
		}
	}

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::White;
	float FinalBarOpacity = BarOpacity;
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 0.95f*CellHeight, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	if (Flag == UTHUDOwner->UTPlayerOwner->GetViewTarget())
	{
		DrawTexture(TextureAtlas, XOffset + Width, YOffset, 0.95f*CellHeight, 36, 36, 188, -36, 65, FinalBarOpacity, BarColor);
	}

	// Draw the Text
	DrawText(FText::FromString("[" + KeyName.ToString() + "]"), XOffset + 4.f, YOffset + ColumnY, UTHUDOwner->TinyFont, 1.f, 1.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f), ETextHorzPos::Left, ETextVertPos::Center);

	FLinearColor FlagColor = Flag->Team ? Flag->Team->TeamColor : FLinearColor::White;
	DrawTexture(FlagIcon.Texture, XOffset + (Width * 0.5f), YOffset + ColumnY - 0.025f*Width, 0.09f*Width, 0.09f*Width, FlagIcon.U, FlagIcon.V, FlagIcon.UL, FlagIcon.VL, 1.0, FlagColor, FVector2D(1.0, 0.0));

	DrawText(FText::FromString(FlagName), XOffset + (Width * 0.6f), YOffset + ColumnY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FlagColor, ETextHorzPos::Left, ETextVertPos::Center);
}

void UUTHUDWidget_SpectatorSlideOut::DrawCamBind(FName KeyName, FString ProjName, float RenderDelta, float XOffset, float YOffset, bool bCamSelected)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = 0.75f * Size.X;

	// If we are interactive and this element has a keybind, store it so the mouse can click it
	if (bIsInteractive && KeyName != NAME_None)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + Width) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));
		ClickElementStack.Add(FClickElement(KeyName, Bounds));
		if ((MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W))
		{
			BarOpacity = 0.5;
		}
	}

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::White;
	float FinalBarOpacity = BarOpacity;
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 0.95f*CellHeight, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	if (bCamSelected)
	{
		DrawTexture(TextureAtlas, XOffset + Width, YOffset, 35, 0.95f*CellHeight, 36, 188, -36, 65, FinalBarOpacity, BarColor);
	}

	// Draw the Text
	if (KeyName != NAME_None)
	{
		DrawText(FText::FromString("[" + KeyName.ToString() + "]"), XOffset + 4.f, YOffset + ColumnY, UTHUDOwner->TinyFont, 1.f, 1.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f), ETextHorzPos::Left, ETextVertPos::Center);
	}
	DrawText(FText::FromString(ProjName), XOffset + (Width * 0.98f), YOffset + ColumnY, UTHUDOwner->TinyFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Right, ETextVertPos::Center);
}

void UUTHUDWidget_SpectatorSlideOut::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	if (PlayerState == NULL) return;

	FLinearColor DrawColor = FLinearColor::White;
	float FinalBarOpacity = 0.3f;
	float Width = Size.X;

	// If we are interactive and this element has a keybind, store it so the mouse can click it
	if (bIsInteractive)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + Width) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));
		ClickElementStack.Add(FClickElement(NumberToKey(Index), Bounds));
		if (MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W)
		{
			FinalBarOpacity = 1.f;
		}
	}

	FText PlayerName = FText::FromString(GetClampedName(PlayerState, SlideOutFont, 1.f, 0.475f*Width));
	FText PlayerScore = FText::AsNumber(int32(PlayerState->Score));

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::Black;
	if (PlayerState->Team)
	{
		BarColor = PlayerState->Team->TeamColor;
		BarColor.R *= 0.5f;
		BarColor.G *= 0.5f;
		BarColor.B *= 0.5f;
	}

	AUTCharacter* Character = PlayerState->GetUTCharacter();
	PlayerState->SpectatorNameScale = 1.f; 
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 0.95f*CellHeight, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	if ((PlayerState == UTHUDOwner->UTPlayerOwner->LastSpectatedPlayerState) || (PlayerState->CarriedObject && (PlayerState->CarriedObject == UTHUDOwner->UTPlayerOwner->GetViewTarget())))
	{
		DrawTexture(TextureAtlas, XOffset + Width, YOffset, 35, 0.95f*CellHeight, 36, 188, -36, 65, FinalBarOpacity, BarColor);
	}

	int32 FlagU=0;
	int32 FlagV=0;

	UTexture2D* NewFlagAtlas = UTHUDOwner->ResolveFlag(PlayerState->CountryFlag, FlagU, FlagV);
	DrawTexture(NewFlagAtlas, XOffset + (Width * FlagX), YOffset + 18, 36, 26, FlagU, FlagV, 36, 26, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));	// Add a function to support additional flags

	// Draw the Text
	if (Index >= 0)
	{
		FText Position = FText::Format(NSLOCTEXT("UTScoreboard", "PositionFormatText", "{0}."), FText::AsNumber(Index));
		DrawText(Position, XOffset + 4.f, YOffset + ColumnY, SlideOutFont, 1.f, 1.f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
	}
	FVector2D NameSize = DrawText(PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY, SlideOutFont, PlayerState->SpectatorNameScale, PlayerState->SpectatorNameScale, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	if (UTGameState && UTGameState->HasMatchStarted())
	{
		if (Character)
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

			DrawTexture(HealthIcon.Texture, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY - 0.015f*Width, 0.07f*Width, 0.07f*Width, HealthIcon.U, HealthIcon.V, HealthIcon.UL, HealthIcon.VL, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
			FFormatNamedArguments Args;
			Args.Add("Health", FText::AsNumber(Character->Health));
			DrawColor = FLinearColor(0.5f, 1.f, 0.5f);
			DrawText(FText::Format(NSLOCTEXT("UTCharacter", "HealthDisplay", "{Health}"), Args), XOffset + (Width * (ColumnHeaderScoreX + 0.06f)), YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);

			if (Character->ArmorAmount > 0)
			{
				DrawTexture(ArmorIcon.Texture, XOffset + (Width * ColumnHeaderArmor), YOffset + ColumnY - 0.015f*Width, 0.065f*Width, 0.065f*Width, ArmorIcon.U, ArmorIcon.V, ArmorIcon.UL, ArmorIcon.VL, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
				FFormatNamedArguments ArmorArgs;
				ArmorArgs.Add("Armor", FText::AsNumber(Character->ArmorAmount));
				DrawColor = FLinearColor(1.f, 1.f, 0.5f);
				DrawText(FText::Format(NSLOCTEXT("UTCharacter", "ArmorDisplay", "{Armor}"), ArmorArgs), XOffset + (Width * (ColumnHeaderArmor + 0.062f)), YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
			}
			if (Character->GetWeaponClass())
			{
				AUTWeapon* DefaultWeapon = Character->GetWeaponClass()->GetDefaultObject<AUTWeapon>();
				if (DefaultWeapon)
				{
					float WeaponScale = 0.5f * (1.f + FMath::Clamp(0.6f - GetWorld()->GetTimeSeconds() + Character->LastWeaponFireTime, 0.f, 0.4f));  
					DrawTexture(WeaponAtlas, XOffset + Width, YOffset + ColumnY - 0.015f*Width, WeaponScale*DefaultWeapon->WeaponBarSelectedUVs.UL, WeaponScale*DefaultWeapon->WeaponBarSelectedUVs.VL, DefaultWeapon->WeaponBarSelectedUVs.U + DefaultWeapon->WeaponBarSelectedUVs.UL, DefaultWeapon->WeaponBarSelectedUVs.V, -1.f * DefaultWeapon->WeaponBarSelectedUVs.UL, DefaultWeapon->WeaponBarSelectedUVs.VL, 1.0, FLinearColor::White);
				}
			}
		}
		else
		{
			// skull
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (Width * ColumnHeaderScoreX), YOffset, 0.075f*Width, 0.075f*Width, 725, 0, 28, 36, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
		}
	}
}

int32 UUTHUDWidget_SpectatorSlideOut::MouseHitTest(FVector2D Position)
{
	if (bIsInteractive)
	{
		for (int32 i = 0; i < ClickElementStack.Num(); i++)
		{
			if (Position.X >= ClickElementStack[i].Bounds.X && Position.X <= ClickElementStack[i].Bounds.Z &&
				Position.Y >= ClickElementStack[i].Bounds.Y && Position.Y <= ClickElementStack[i].Bounds.W)
			{
				return i;
			}
		}
	}
	return -1;
}

bool UUTHUDWidget_SpectatorSlideOut::MouseClick(FVector2D InMousePosition)
{ 
	int32 ElementIndex = MouseHitTest(InMousePosition);
	if (ClickElementStack.IsValidIndex(ElementIndex) && UTHUDOwner && UTHUDOwner->PlayerOwner && Cast<UUTPlayerInput>(UTHUDOwner->PlayerOwner->PlayerInput))
	{
		UUTPlayerInput* Input = Cast<UUTPlayerInput>(UTHUDOwner->PlayerOwner->PlayerInput);
		Input->ExecuteCustomBind(ClickElementStack[ElementIndex].Key, EInputEvent::IE_Pressed);
		return true;
	}
	return false; 
}

FKey UUTHUDWidget_SpectatorSlideOut::NumberToKey(int32 InNumber)
{
	switch (InNumber)
	{
	case 0: return FKey(TEXT("Zero"));
	case 1: return FKey(TEXT("One"));
	case 2: return FKey(TEXT("Two"));
	case 3: return FKey(TEXT("Three"));
	case 4: return FKey(TEXT("Four"));
	case 5: return FKey(TEXT("Five"));
	case 6: return FKey(TEXT("Six"));
	case 7: return FKey(TEXT("Seven"));
	case 8: return FKey(TEXT("Eight"));
	case 9: return FKey(TEXT("Nine"));
	}
	return FKey();
}