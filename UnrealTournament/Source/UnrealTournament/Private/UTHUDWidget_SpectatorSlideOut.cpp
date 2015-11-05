// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTPlayerInput.h"
#include "UTCTFGameState.h"
#include "UTSpectatorCamera.h"
#include "UTPickupInventory.h"
#include "UTArmor.h"
#include "UTDemoRecSpectator.h"
#include "UTWeap_ImpactHammer.h"
#include "UTWeap_Enforcer.h"
#include "UTWeap_Translocator.h"

UUTHUDWidget_SpectatorSlideOut::UUTHUDWidget_SpectatorSlideOut(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(320.0f, 108.0f);
	ScreenPosition = FVector2D(0.0f, 0.005f);
	Origin = FVector2D(0.0f, 0.0f);
	ArrowSize = 36.f / Size.X;

	FlagX = 0.02f;
	ColumnHeaderPlayerX = 0.12f;
	ColumnHeaderScoreX = 0.78f;
	ColumnHeaderArmor = 0.93f;
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

	CamTypeButtonStart = 0.75f * ArrowSize;
	CamTypeButtonWidth = 0.1f;
	MouseOverOpacity = 0.5f;
	SelectedOpacity = 0.7f;
	CameraBindWidth = 0.48f;
	bShowingStats = false;
	KillsColumn = 0.32f;
	DeathsColumn = 0.49f;
	ShotsColumn = 0.66f;
	AccuracyColumn = 0.83f;
}

bool UUTHUDWidget_SpectatorSlideOut::ShouldDraw_Implementation(bool bShowScores)
{
	if (!bShowScores && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTGameState && UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
	{
		if (UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted() || UTGameState->IsMatchAtHalftime())
		{
			return false;
		}

#if !UE_SERVER
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(UTHUDOwner->UTPlayerOwner->Player);
		if (LP && LP->bRecordingReplay)
		{
			return false;
		}
#endif
		return true;
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

		float DrawOffset = 0.2f * Canvas->ClipY/RenderScale;
		if (SlideIn <= 0.f)
		{
			DrawSelector("ToggleSlideOut", true, 0.f, DrawOffset);
			return;
		}

		DrawSelector("ToggleMinimap", !UTHUDOwner->bDrawMinimap, 0.f, DrawOffset - CellHeight);

		float XOffset = SlideIn - Size.X;
		SlideOutFont = UTHUDOwner->SmallFont;

		DrawPlayerHeader(DeltaTime, XOffset, DrawOffset);
		if (SlideIn > 0.95f)
		{
			DrawSelector("ToggleSlideOut", false, 0.f, DrawOffset);
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
		int32 NumRed = UTGameState->bTeamGame ? FMath::Min(RedPlayerList.Num(), 12) : FMath::Min(RedPlayerList.Num(), 24);
		for (int32 i = 0; i < NumRed; i++)
		{
			DrawPlayer(i, RedPlayerList[i], DeltaTime, XOffset, DrawOffset);
			DrawOffset += CellHeight;
		}

		if (UTGameState->bTeamGame)
		{
			DrawOffset += CellHeight;
			BluePlayerList.Sort(SortFunc);
			int32 NumBlue = FMath::Min(BluePlayerList.Num(), 12);
			for (int32 i = 0; i < NumBlue; i++)
			{
				DrawPlayer(i, BluePlayerList[i], DeltaTime, XOffset, DrawOffset);
				DrawOffset += CellHeight;
			}
		}
		DrawOffset += 0.2f*CellHeight;

		AUTCTFGameState * CTFGameState = Cast<AUTCTFGameState>(UTGameState);
		if (CTFGameState && (CTFGameState->FlagBases.Num() > 1))
		{
			// show flag binds
			if (CTFGameState->FlagBases[0] && CTFGameState->FlagBases[0]->MyFlag)
			{
				DrawFlag("ViewFlag 0", "Red", CTFGameState->FlagBases[0]->MyFlag, DeltaTime, XOffset + CamTypeButtonStart*Size.X, DrawOffset);
			}
			if (CTFGameState->FlagBases[1] && CTFGameState->FlagBases[1]->MyFlag)
			{
				DrawFlag("ViewFlag 1", "Blue", CTFGameState->FlagBases[1]->MyFlag, DeltaTime, XOffset + CamTypeButtonStart*Size.X + 0.37f * Size.X, DrawOffset);
			}
			DrawOffset += 1.2f*CellHeight;
		}

		//draw powerup clocks
		DrawSelector("ToggleShowTimers", !UTHUDOwner->UTPlayerOwner->bShowPowerupTimers, XOffset, DrawOffset);
		if (UTHUDOwner->UTPlayerOwner->bShowPowerupTimers)
		{
			if (!bPowerupListInitialized)
			{
				InitPowerupList();
			}
			int32 TimerOffset = XOffset + CamTypeButtonStart*Size.X;
			for (int32 i = 0; i < PowerupList.Num(); i++)
			{
				if (PowerupList[i] != NULL)
				{
					DrawPowerup(PowerupList[i], TimerOffset, DrawOffset);
					if (i % 2 == 0)
					{
						TimerOffset = XOffset + CamTypeButtonStart*Size.X + 0.37f * Size.X;
					}
					else
					{
						TimerOffset = XOffset + CamTypeButtonStart*Size.X;
						DrawOffset += 1.2f*CellHeight;
					}
				}
			}
		}
		DrawOffset += 1.2f*CellHeight;

		float StartCamOffset = DrawOffset;
		float EndCamOffset = 0.0f; 
		bool bOverflow = false;

		DrawSelector("ToggleShowBinds", !UTHUDOwner->UTPlayerOwner->bShowCameraBinds, XOffset, DrawOffset);
		float CamOffset = XOffset + CamTypeButtonStart*Size.X;

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

			bOverflow = false;
			StartCamOffset = DrawOffset;
			EndCamOffset = 0.0f; //Unknown. will be filled in when cambinds hit the bottom of the screen
			NumCamBinds = NumCameras + ((Cast<AUTDemoRecSpectator>(UTHUDOwner->PlayerOwner) != nullptr) ? 5 : 0);
			DrawnCamBinds = 0;
			for (int32 i = 0; i < Input->SpectatorBinds.Num(); i++)
			{
				if ((Input->SpectatorBinds[i].FriendlyName != "") && (Input->SpectatorBinds[i].Command != ""))
				{
					DrawCamBind(Input->SpectatorBinds[i].Command, Input->SpectatorBinds[i].FriendlyName, DeltaTime, CamOffset, DrawOffset, CameraBindWidth * Size.X, (Cast<AUTProjectile>(UTHUDOwner->UTPlayerOwner->GetViewTarget()) != NULL));
					UpdateCameraBindOffset(DrawOffset, CamOffset, bOverflow, StartCamOffset, EndCamOffset);
					NumCamBinds++;
				}
			}
			bBalanceCamBinds = (StartCamOffset + float(NumCamBinds)*CellHeight > Canvas->ClipY / RenderScale);

			if (Cast<AUTDemoRecSpectator>(UTHUDOwner->PlayerOwner) != nullptr)
			{
				DrawCamBind("DemoRestart", "Restart Demo", DeltaTime, CamOffset, DrawOffset, CameraBindWidth * Size.X, false);
				UpdateCameraBindOffset(DrawOffset, CamOffset, bOverflow, StartCamOffset, EndCamOffset);

				DrawCamBind("DemoSeek -5", "Rewind Demo", DeltaTime, CamOffset, DrawOffset, CameraBindWidth * Size.X, false);
				UpdateCameraBindOffset(DrawOffset, CamOffset, bOverflow, StartCamOffset, EndCamOffset);

				DrawCamBind("DemoSeek 10", "Fast Forward", DeltaTime, CamOffset, DrawOffset, CameraBindWidth * Size.X, false);
				UpdateCameraBindOffset(DrawOffset, CamOffset, bOverflow, StartCamOffset, EndCamOffset);

				DrawCamBind("DemoGoToLive", "Jump to Real Time", DeltaTime, CamOffset, DrawOffset, CameraBindWidth * Size.X, false);
				UpdateCameraBindOffset(DrawOffset, CamOffset, bOverflow, StartCamOffset, EndCamOffset);

				DrawCamBind("DemoPause", "Pause Demo", DeltaTime, CamOffset, DrawOffset, CameraBindWidth * Size.X, false);
				UpdateCameraBindOffset(DrawOffset, CamOffset, bOverflow, StartCamOffset, EndCamOffset);
			}

			for (int32 i = 0; i < NumCameras; i++)
			{
				DrawCamBind("ViewCamera " + FString::Printf(TEXT("%d"), i), CameraString[i], DeltaTime, CamOffset, DrawOffset, CameraBindWidth * Size.X, false);
				UpdateCameraBindOffset(DrawOffset, CamOffset, bOverflow, StartCamOffset, EndCamOffset);
			}
		}
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawPowerup(AUTPickup* Pickup, float XOffset, float YOffset)
{
	// @TODO FIXMESTEVE get rid of armor hacks when they have icons
	FLinearColor BarColor = FLinearColor::White;
	float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(Pickup->WakeUpTimerHandle);
	float BarOpacity = (RemainingTime > 0.f) ? 0.3f : 0.6f;

	// If we are interactive and this element has a keybind, store it so the mouse can click it
	if (bIsInteractive)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + 0.35f*Size.X) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));
		ClickElementStack.Add(FClickElement("ViewPowerup " + FString::Printf(TEXT("%s"), *Pickup->GetName()), Bounds));
		if ((MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W))
		{
			BarOpacity = MouseOverOpacity;
		}
	}
	if (UTHUDOwner->UTPlayerOwner && (UTHUDOwner->UTPlayerOwner->GetViewTarget() == Pickup))
	{
		BarOpacity = SelectedOpacity;
	}
	DrawTexture(TextureAtlas, XOffset, YOffset, 0.35f*Size.X, 0.95f*CellHeight, 149, 138, 32, 32, BarOpacity, FLinearColor::White);

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
	else if (PickupInventory && PickupInventory->GetInventoryType()->IsChildOf(AUTWeapon::StaticClass()))
	{
		FCanvasIcon HUDIcon = PickupInventory->HUDIcon;
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
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "PowerupTimeDisplay", "{TimeRemaining}"), Args), XOffset + 0.29f*Size.X, YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	else
	{
		DrawText(NSLOCTEXT("UTCharacter", "PowerupUp", "UP"), XOffset + 0.28f*Size.X, YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawFlag(FString FlagCommand, FString FlagName, AUTCarriedObject* Flag, float RenderDelta, float XOffset, float YOffset)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = 0.35f * Size.X;

	// If we are interactive and this element has a keybind, store it so the mouse can click it
	if (bIsInteractive)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + Width) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));
		ClickElementStack.Add(FClickElement(FlagCommand, Bounds));
		if (MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W)
		{
			BarOpacity = MouseOverOpacity;
		}
		if (UTHUDOwner->PlayerOwner && (UTHUDOwner->PlayerOwner->GetViewTarget() == Flag))
		{
			BarOpacity = SelectedOpacity;
		}
	}

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::White;
	float FinalBarOpacity = BarOpacity;
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 0.95f*CellHeight, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	// Draw the Text
	FLinearColor FlagColor = Flag->Team ? Flag->Team->TeamColor : FLinearColor::White;
	DrawTexture(FlagIcon.Texture, XOffset + (Width * 0.25f), YOffset + ColumnY - 0.1f*Width, 0.25f*Width, 0.25f*Width, FlagIcon.U, FlagIcon.V, FlagIcon.UL, FlagIcon.VL, 1.0, FlagColor, FVector2D(1.0, 0.0));

	DrawText(FText::FromString(FlagName), XOffset + (Width * 0.28f), YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, FlagColor, ETextHorzPos::Left, ETextVertPos::Center);
}

void UUTHUDWidget_SpectatorSlideOut::DrawCamBind(FString CamCommand, FString ProjName, float RenderDelta, float XOffset, float YOffset, float Width, bool bCamSelected)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;

	// If we are interactive and this element has a keybind, store it so the mouse can click it
	if (bIsInteractive)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + Width) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));
		ClickElementStack.Add(FClickElement(CamCommand, Bounds));
		if ((MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W))
		{
			BarOpacity = MouseOverOpacity;
		}
	}
	if (bCamSelected)
	{
		BarOpacity = SelectedOpacity;
	}

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::White;
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 0.95f*CellHeight, 149, 138, 32, 32, BarOpacity, BarColor);

	// Draw the Text
	float XL, YL;
	Canvas->TextSize(UTHUDOwner->TinyFont, ProjName, XL, YL, 1.f, 1.f);
	DrawText(FText::FromString(ProjName), XOffset + 0.5f*(Width-XL), YOffset + ColumnY, UTHUDOwner->TinyFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
}

void UUTHUDWidget_SpectatorSlideOut::UpdateCameraBindOffset(float& DrawOffset, float& XOffset, bool& bOverflow, float StartOffset, float& EndCamOffset)
{
	DrawnCamBinds++;
	if (!bOverflow)
	{
		DrawOffset += CellHeight;
		if ((DrawOffset > (Canvas->ClipY / RenderScale) - CellHeight) || (bBalanceCamBinds && (NumCamBinds <= 2 * DrawnCamBinds)))
		{
			bOverflow = true;
			XOffset = XOffset + 0.51f * Size.X;
			DrawOffset -= CellHeight;
			EndCamOffset = DrawOffset;
			DrawnCamBinds = 0;
		}
	}
	else
	{
		//Once we overflow once, always start at the bottom and work our way up
		DrawOffset -= CellHeight;
		if (DrawOffset < StartOffset)
		{
			XOffset = XOffset + 0.51f * Size.X;
			DrawOffset = EndCamOffset;
		}
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawPlayerHeader(float RenderDelta, float XOffset, float YOffset)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = Size.X;
	FLinearColor BarColor = FLinearColor::White;
	float FinalBarOpacity = BarOpacity;

	if (bIsInteractive && UTHUDOwner->UTPlayerOwner)
	{
		FText CamString = UTHUDOwner->UTPlayerOwner->bSpectateBehindView ? NSLOCTEXT("UTSlideout", "CamType3P", "3P") : NSLOCTEXT("UTSlideout", "CamType1P", "1P");
		float Spacing = 0.333f * (ColumnHeaderScoreX - 0.05f - CamTypeButtonStart - 2.7f * CamTypeButtonWidth - 0.3f);
		DrawCamBind("ToggleBehindView", CamString.ToString(), RenderDelta, XOffset + CamTypeButtonStart*Width, YOffset, CamTypeButtonWidth * Size.X, false);
		DrawCamBind("ToggleTacCom", "X-Ray", RenderDelta, XOffset + (CamTypeButtonStart + CamTypeButtonWidth + Spacing)*Width, YOffset, 1.7f* CamTypeButtonWidth * Size.X, Cast<AUTPlayerController>(UTHUDOwner->UTPlayerOwner) && Cast<AUTPlayerController>(UTHUDOwner->UTPlayerOwner)->bTacComView);
		DrawCamBind("EnableAutoCam", "Auto Cam", RenderDelta, XOffset + (CamTypeButtonStart + 2.7f * CamTypeButtonWidth + 2.f*Spacing) * Size.X, YOffset, 0.3f * Size.X, Cast<AUTPlayerController>(UTHUDOwner->UTPlayerOwner) && Cast<AUTPlayerController>(UTHUDOwner->UTPlayerOwner)->bAutoCam);
	}
	
	// Draw the background border.
	float BorderWidth = Width * (1.f - ColumnHeaderScoreX + 0.05f);
	DrawTexture(TextureAtlas, XOffset + Width - BorderWidth, YOffset, BorderWidth, 0.95f*CellHeight, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	// Draw the Text
	DrawTexture(HealthIcon.Texture, XOffset + (Width * (ColumnHeaderScoreX - 0.05f)), YOffset + ColumnY - 0.04f*Width, 0.1f*Width, 0.1f*Width, HealthIcon.U, HealthIcon.V, HealthIcon.UL, HealthIcon.VL, 1.0, FLinearColor::White);
	DrawTexture(ArmorIcon.Texture, XOffset + (Width * (ColumnHeaderArmor - 0.05f)), YOffset + ColumnY - 0.04f*Width, 0.1f*Width, 0.1f*Width, ArmorIcon.U, ArmorIcon.V, ArmorIcon.UL, ArmorIcon.VL, 1.0, FLinearColor::White);
}

void UUTHUDWidget_SpectatorSlideOut::DrawSelector(FString Command, bool bPointRight, float XOffset, float YOffset)
{
	if (bIsInteractive)
	{
		FLinearColor DrawColor = FLinearColor::White;
		float U = bPointRight ? 36.f : 0.f;
		float UL = bPointRight ? -36.f : 36.f;
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + CamTypeButtonStart*Size.X) * RenderScale), RenderPosition.Y + ((YOffset + ArrowSize*Size.X) * RenderScale));
		ClickElementStack.Add(FClickElement(Command, Bounds));
		float Opacity = (MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W)
				? 1.f : 0.5f;
		DrawTexture(TextureAtlas, XOffset, YOffset + 0.5f*CellHeight - 9.f, 18.f, 18.f, U, 188.f, UL, 65.f, Opacity, DrawColor);
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	if (PlayerState == NULL) return;

	FLinearColor DrawColor = FLinearColor::White;
	float FinalBarOpacity = 0.3f;
	float Width = Size.X;

	// If we are interactive and this element has a keybind, store it so the mouse can click it
	if (bIsInteractive && UTGameState)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
			RenderPosition.X + ((XOffset + Width) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));
		int32 PickedTeamNum = (PlayerState && PlayerState->Team) ? PlayerState->Team->TeamIndex : 255;
		int32 SpectatingID = UTGameState->bTeamGame ? PlayerState->SpectatingIDTeam : PlayerState->SpectatingID;
		ClickElementStack.Add(FClickElement("ViewPlayerNum " + FString::Printf(TEXT("%d %d"), SpectatingID, PickedTeamNum), Bounds, PlayerState));
		if (MousePosition.X >= Bounds.X && MousePosition.X <= Bounds.Z && MousePosition.Y >= Bounds.Y && MousePosition.Y <= Bounds.W)
		{
			FinalBarOpacity = MouseOverOpacity;
		}
	}
	bool bIsSelectedPlayer = false;
	if (Cast<AUTPlayerController>(UTHUDOwner->PlayerOwner) && (Cast<AUTPlayerController>(UTHUDOwner->PlayerOwner)->LastSpectatedPlayerId == PlayerState->SpectatingID))
	{
		FinalBarOpacity = SelectedOpacity;
		bIsSelectedPlayer = true;
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
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 0.95f*CellHeight, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	FTextureUVs FlagUV;
	UTexture2D* NewFlagAtlas = UTHUDOwner->ResolveFlag(PlayerState, FlagUV);
	DrawTexture(NewFlagAtlas, XOffset + (Width * FlagX), YOffset + 18, FlagUV.UL, FlagUV.VL, FlagUV.U, FlagUV.V, 36, 26, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));

	FVector2D NameSize = DrawText(PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY, SlideOutFont, 1.f, 1.f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	if (bShowingStats && bIsSelectedPlayer)
	{
		ShowSelectedPlayerStats(PlayerState, RenderDelta, XOffset + 1.2f * Width, YOffset);
	}
	if (UTGameState && UTGameState->HasMatchStarted())
	{
		if (Character && (Character->Health > 0))
		{
			float FlagOffset = -0.05f;
			if (Character->GetWeaponOverlayFlags() != 0)
			{
				// @TODO FIXMESTEVE - support actual overlays for different powerups - save related class when setting up overlays in GameState, so have easy mapping. 
				// For now just assume UDamage
				DrawTexture(UDamageHUDIcon.Texture, XOffset + (Width * (ColumnHeaderScoreX - 0.05f)), YOffset + ColumnY - 0.05f*Width, 0.1f*Width, 0.1f*Width, UDamageHUDIcon.U, UDamageHUDIcon.V, UDamageHUDIcon.UL, UDamageHUDIcon.VL, 1.0, FLinearColor::White, FVector2D(1.0, 0.0));
				FlagOffset = -0.15f;
			}
			if (PlayerState->CarriedObject)
			{
				FLinearColor FlagColor = PlayerState->CarriedObject->Team ? PlayerState->CarriedObject->Team->TeamColor : FLinearColor::White;
				DrawTexture(FlagIcon.Texture, XOffset + (Width * (ColumnHeaderScoreX + FlagOffset)), YOffset + ColumnY - 0.025f*Width, 0.09f*Width, 0.09f*Width, FlagIcon.U, FlagIcon.V, FlagIcon.UL, FlagIcon.VL, 1.0, FlagColor, FVector2D(1.0, 0.0));
			}

			FFormatNamedArguments Args;
			Args.Add("Health", FText::AsNumber(Character->Health));
			DrawColor = FLinearColor(0.5f, 1.f, 0.5f);
			DrawText(FText::Format(NSLOCTEXT("UTCharacter", "HealthDisplay", "{Health}"), Args), XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);

			if (Character->ArmorAmount > 0)
			{
				FFormatNamedArguments ArmorArgs;
				ArmorArgs.Add("Armor", FText::AsNumber(Character->ArmorAmount));
				DrawColor = FLinearColor(1.f, 1.f, 0.5f);
				DrawText(FText::Format(NSLOCTEXT("UTCharacter", "ArmorDisplay", "{Armor}"), ArmorArgs), XOffset + (Width * ColumnHeaderArmor), YOffset + ColumnY, SlideOutFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
			}
			if (Character->GetWeaponClass())
			{
				AUTWeapon* DefaultWeapon = Character->GetWeaponClass()->GetDefaultObject<AUTWeapon>();
				if (DefaultWeapon)
				{
					float WeaponScale = 0.5f * (1.f + FMath::Clamp(0.6f - GetWorld()->GetTimeSeconds() + Character->LastWeaponFireTime, 0.f, 0.4f));  
					DrawTexture(WeaponAtlas, XOffset + 1.01f*Width, YOffset + ColumnY - 0.015f*Width, WeaponScale*DefaultWeapon->WeaponBarSelectedUVs.UL, WeaponScale*DefaultWeapon->WeaponBarSelectedUVs.VL, DefaultWeapon->WeaponBarSelectedUVs.U + DefaultWeapon->WeaponBarSelectedUVs.UL, DefaultWeapon->WeaponBarSelectedUVs.V, -1.f * DefaultWeapon->WeaponBarSelectedUVs.UL, DefaultWeapon->WeaponBarSelectedUVs.VL, 1.0, FLinearColor::White);
				}
			}
		}
		else
		{
			// skull
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + Width * ColumnHeaderScoreX - 0.0375*Width, YOffset, 0.075f*Width, 0.075f*Width, 725, 0, 28, 36, 1.0, FLinearColor::White);
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
	if (ClickElementStack.IsValidIndex(ElementIndex) && UTHUDOwner && Cast<AUTPlayerController>(UTHUDOwner->PlayerOwner) && Cast<UUTPlayerInput>(UTHUDOwner->PlayerOwner->PlayerInput))
	{
		if (ClickElementStack[ElementIndex].SelectedPlayer && (Cast<AUTPlayerController>(UTHUDOwner->PlayerOwner)->LastSpectatedPlayerId == ClickElementStack[ElementIndex].SelectedPlayer->SpectatingID))
		{
			ToggleStats();
			return true;
		}
		FStringOutputDevice DummyOut;
		UTHUDOwner->PlayerOwner->Player->Exec(UTHUDOwner->PlayerOwner->GetWorld(), *ClickElementStack[ElementIndex].Command, DummyOut);
		return true;
	}
	return false; 
}

float UUTHUDWidget_SpectatorSlideOut::GetDrawScaleOverride()
{
	return 1.f;
}

void UUTHUDWidget_SpectatorSlideOut::ToggleStats()
{
	bShowingStats = !bShowingStats;
}

void UUTHUDWidget_SpectatorSlideOut::ShowSelectedPlayerStats(AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	FStatsFontInfo StatsFontInfo;
	StatsFontInfo.TextRenderInfo.bEnableShadow = true;
	StatsFontInfo.TextRenderInfo.bClipText = true;
	StatsFontInfo.TextFont = UTHUDOwner->TinyFont;
	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->TinyFont, "TEST", XL, SmallYL, 1.f, 1.f);
	StatsFontInfo.TextHeight = SmallYL;

	float ScoreWidth = 0.4f * Canvas->ClipX;
	float MaxHeight = 0.5f*Canvas->ClipY;
	float ScoreYOffset = FMath::Min(YOffset, Canvas->ClipY - MaxHeight);
	DrawTexture(TextureAtlas, XOffset, ScoreYOffset, ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.3f, FLinearColor::Black);
	DrawWeaponStats(PlayerState, RenderDelta, ScoreYOffset, XOffset, ScoreWidth, MaxHeight, StatsFontInfo);
}

void UUTHUDWidget_SpectatorSlideOut::DrawWeaponStatsLine(FText StatsName, int32 StatValue, int32 ScoreValue, int32 Shots, float Accuracy, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth, bool bIsBestWeapon)
{
	FLinearColor DrawColor = bIsBestWeapon ? FLinearColor::Yellow : FLinearColor::White;
	DrawText(StatsName, XOffset, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, DrawColor, ETextHorzPos::Left, ETextVertPos::Top);

	if (StatValue >= 0)
	{
		DrawColor = (StatValue >= 15) ? FLinearColor::Yellow : FLinearColor::White;
		FFormatNamedArguments StatArgs;
		StatArgs.Add("Stat", FText::AsNumber(StatValue));
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "StatDisplay", "{Stat}"), StatArgs), XOffset + KillsColumn*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, DrawColor, ETextHorzPos::Center, ETextVertPos::Top);
	}
	DrawColor = FLinearColor::White;
	if (ScoreValue >= 0)
	{
		FFormatNamedArguments StatArgs;
		StatArgs.Add("Stat", FText::AsNumber(ScoreValue));
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "StatDisplay", "{Stat}"), StatArgs), XOffset + DeathsColumn*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, DrawColor, ETextHorzPos::Center, ETextVertPos::Top);
	}
	if (Shots >= 0)
	{
		FFormatNamedArguments StatArgs;
		StatArgs.Add("Stat", FText::AsNumber(Shots));
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "StatDisplay", "{Stat}"), StatArgs), XOffset + ShotsColumn*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, DrawColor, ETextHorzPos::Center, ETextVertPos::Top);

		FNumberFormattingOptions NumberFormattingOptions;
		NumberFormattingOptions.MaximumFractionalDigits = 1;
		FFormatNamedArguments AccArgs;
		AccArgs.Add("Stat", FText::AsNumber(Accuracy, &NumberFormattingOptions));
		DrawText(FText::Format(NSLOCTEXT("UTCharacter", "StatDisplay", "{Stat}"), AccArgs), XOffset + AccuracyColumn*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, DrawColor, ETextHorzPos::Center, ETextVertPos::Top);
	}
	YPos += StatsFontInfo.TextHeight;
}

void UUTHUDWidget_SpectatorSlideOut::DrawWeaponStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo)
{
	DrawText(NSLOCTEXT("SlideOut", "KillsW", "Kills w /"), XOffset + (KillsColumn - 0.05f)*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
	DrawText(NSLOCTEXT("SlideOut", "DeathsBy", "Deaths by"), XOffset + (DeathsColumn - 0.05f)*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
	DrawText(NSLOCTEXT("SlideOut", "Shots", "Shots"), XOffset + (ShotsColumn - 0.02f)*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
	DrawText(NSLOCTEXT("SlideOut", "Accuracy", "Accuracy"), XOffset + (AccuracyColumn - 0.03f)*ScoreWidth, YPos, UTHUDOwner->TinyFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
	YPos += StatsFontInfo.TextHeight;

	/** List of weapons to display stats for. */
	if (StatsWeapons.Num() == 0)
	{
		// add default weapons - needs to be automated
		StatsWeapons.AddUnique(AUTWeap_ImpactHammer::StaticClass()->GetDefaultObject<AUTWeapon>());
		StatsWeapons.AddUnique(AUTWeap_Enforcer::StaticClass()->GetDefaultObject<AUTWeapon>());

		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTPickupWeapon* Pickup = Cast<AUTPickupWeapon>(*It);
			if (Pickup && Pickup->GetInventoryType())
			{
				StatsWeapons.AddUnique(Pickup->GetInventoryType()->GetDefaultObject<AUTWeapon>());
			}
		}

		StatsWeapons.AddUnique(AUTWeap_Translocator::StaticClass()->GetDefaultObject<AUTWeapon>());
	}

	float BestWeaponKills = (BestWeaponIndex == FMath::Clamp(BestWeaponIndex, 0, StatsWeapons.Num() - 1)) ? StatsWeapons[BestWeaponIndex]->GetWeaponKillStats(PS) : 0;
	for (int32 i = 0; i < StatsWeapons.Num(); i++)
	{
		int32 Kills = StatsWeapons[i]->GetWeaponKillStats(PS);
		float Shots = StatsWeapons[i]->GetWeaponShotsStats(PS);
		float Accuracy = (Shots > 0) ? 100.f * StatsWeapons[i]->GetWeaponHitsStats(PS) / Shots : 0.f;
		DrawWeaponStatsLine(StatsWeapons[i]->DisplayName, Kills, StatsWeapons[i]->GetWeaponDeathStats(PS), Shots, Accuracy, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, (i == BestWeaponIndex));
		if (Kills > BestWeaponKills)
		{
			BestWeaponKills = Kills;
			BestWeaponIndex = i;
		}
	}
}
