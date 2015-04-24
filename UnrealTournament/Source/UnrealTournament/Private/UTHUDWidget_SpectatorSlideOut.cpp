// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTPlayerInput.h"
#include "UTCTFGameState.h"

UUTHUDWidget_SpectatorSlideOut::UUTHUDWidget_SpectatorSlideOut(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(500.0f, 108.0f);
	ScreenPosition = FVector2D(0.0f, 0.1f);
	Origin = FVector2D(0.0f, 0.0f);

	FlagX = 0.09;
	ColumnHeaderPlayerX = 0.16;
	ColumnHeaderScoreX = 0.7;
	ColumnHeaderArmor = 0.87f;
	ColumnY = 12;

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

}

bool UUTHUDWidget_SpectatorSlideOut::ShouldDraw_Implementation(bool bShowScores)
{
	if (!bShowScores && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTGameState && UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
	{
		if (UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted() || UTGameState->IsMatchAtHalftime())
		{
			return false;
		}
		return (UTHUDOwner->UTPlayerOwner->bRequestingSlideOut || (SlideIn > 0.f));
	}
	return false;
}

void UUTHUDWidget_SpectatorSlideOut::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	if (TextureAtlas && UTGameState)
	{
		SlideIn = UTHUDOwner->UTPlayerOwner->bRequestingSlideOut ? FMath::Min(Size.X, SlideIn + DeltaTime*Size.X*SlideSpeed) : FMath::Max(0.f, SlideIn - DeltaTime*Size.X*SlideSpeed);

		int32 Place = 1;
		int32 MaxRedPlaces = UTGameState->bTeamGame ? 5 : 10;
		int32 XOffset = SlideIn - Size.X;
		float DrawOffset = 0.f;
		UTGameState->FillPlayerLists();
		for (int32 i = 0; i<UTGameState->RedPlayerList.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->RedPlayerList[i]);
			if (PlayerState)
			{
				DrawPlayer(Place, PlayerState, DeltaTime, XOffset, DrawOffset);
				DrawOffset += CellHeight;
				Place++;
				if (Place > MaxRedPlaces)
				{
					break;
				}
			}
		}
		if (UTGameState->bTeamGame)
		{
			Place = MaxRedPlaces + 1;
			for (int32 i = 0; i<UTGameState->BluePlayerList.Num(); i++)
			{
				AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->BluePlayerList[i]);
				if (PlayerState)
				{
					DrawPlayer(Place, PlayerState, DeltaTime, XOffset, DrawOffset);
					DrawOffset += CellHeight;
					Place++;
					if (Place > 10)
					{
						break;
					}
				}
			}
		}

		DrawOffset += CellHeight;
		DrawOffset += CellHeight;
		AUTCTFGameState * CTFGameState = Cast<AUTCTFGameState>(UTGameState);
		if (CTFGameState && (CTFGameState->FlagBases.Num() > 1))
		{
			// show flag binds
			UUTPlayerInput* Input = Cast<UUTPlayerInput>(UTHUDOwner->PlayerOwner->PlayerInput);
			if (Input)
			{
				for (int32 i = 0; i < Input->SpectatorBinds.Num(); i++)
				{
					if (Input->SpectatorBinds[i].Command == "ViewRedFlag")
					{
						if (CTFGameState->FlagBases[0] && CTFGameState->FlagBases[0]->MyFlag)
						{
							DrawFlag(Input->SpectatorBinds[i].KeyName, "Red Flag", CTFGameState->FlagBases[0]->MyFlag, DeltaTime, XOffset, DrawOffset);
							DrawOffset += CellHeight;
						}
					}
					else if (Input->SpectatorBinds[i].Command == "ViewBlueFlag")
					{
						if (CTFGameState->FlagBases[1] && CTFGameState->FlagBases[1]->MyFlag)
						{
							DrawFlag(Input->SpectatorBinds[i].KeyName, "Blue Flag", CTFGameState->FlagBases[1]->MyFlag, DeltaTime, XOffset, DrawOffset);
							DrawOffset += CellHeight;
						}
					}
				}
			}
		}
		DrawOffset += CellHeight;
		DrawOffset += CellHeight;
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawFlag(FName KeyName, FString FlagName, AUTCarriedObject* Flag, float RenderDelta, float XOffset, float YOffset)
{
	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = 0.8f * Size.X;

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::White;
	float FinalBarOpacity = BarOpacity;
	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 36, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	if (Flag == UTHUDOwner->UTPlayerOwner->GetViewTarget())
	{
		DrawTexture(TextureAtlas, XOffset + Width, YOffset, 35, 36, 36, 188, -36, 65, FinalBarOpacity, BarColor);
	}

	// Draw the Text
	DrawText(FText::FromString("[" + KeyName.ToString() + "]"), XOffset + 4.f, YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	FLinearColor FlagColor = Flag->Team ? Flag->Team->TeamColor : FLinearColor::White;
	DrawTexture(FlagIcon.Texture, XOffset + (Width * 0.5f), YOffset + ColumnY - 0.025f*Width, 0.09f*Width, 0.09f*Width, FlagIcon.U, FlagIcon.V, FlagIcon.UL, FlagIcon.VL, 1.0, FlagColor, FVector2D(1.0, 0.0));

	DrawText(FText::FromString(FlagName), XOffset + (Width * 0.6f), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, FlagColor, ETextHorzPos::Left, ETextVertPos::Center);
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

	if (PlayerState == UTHUDOwner->UTPlayerOwner->LastSpectatedPlayerState)
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
