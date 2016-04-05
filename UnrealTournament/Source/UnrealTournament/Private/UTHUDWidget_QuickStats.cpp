// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_QuickStats.h"
#include "UTArmor.h"
#include "UTJumpBoots.h"
#include "UTWeapon.h"
#include "UTTimedPowerup.h"

const float BOUNCE_SCALE = 1.6f;			
const float BOUNCE_TIME = 1.2f;		// SHould be (BOUNCE_SCALE - 1.0) * # of seconds

UUTHUDWidget_QuickStats::UUTHUDWidget_QuickStats(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Position=FVector2D(0.0f, 0.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.5f, 0.5f);
	Origin=FVector2D(0.f,0.0f);
	DesignedResolution=1080;
}

void UUTHUDWidget_QuickStats::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	DrawAngle = 0.0f;
	CurrentLayoutIndex = 0;

	HealthInfo.TextColor = FLinearColor(0.4f, 0.95f, 0.48f, 1.0f);
	HealthInfo.IconColor = HealthInfo.TextColor;

	FlagInfo.bNoText = true;

}

bool UUTHUDWidget_QuickStats::ShouldDraw_Implementation(bool bShowScores)
{
	if (UTGameState && (UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted() || UTGameState->IsMatchIntermission()))
	{
		return false;
	}
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	return (!bShowScores && UTC && !UTC->IsDead() && !UTHUDOwner->GetQuickStatsHidden());
}

FVector2D UUTHUDWidget_QuickStats::CalcDrawLocation(float DistanceInPixels, float Angle)
{
	float Sin = 0.f;
	float Cos = 0.f;

	FMath::SinCos(&Sin,&Cos, FMath::DegreesToRadians(Angle));
	FVector2D NewPoint;

	NewPoint.X = DistanceInPixels * Sin;
	NewPoint.Y = -1.0f * DistanceInPixels * Cos;
	return NewPoint;
}

FVector2D UUTHUDWidget_QuickStats::CalcRotOffset(FVector2D InitialPosition, float Angle)
{
	float Sin = 0.f;
	float Cos = 0.f;

	FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(Angle - 180.0f));
	FVector2D NewPoint;
	
	NewPoint.X = InitialPosition.X * Cos - InitialPosition.Y * Sin;
	NewPoint.Y = InitialPosition.Y * Cos + InitialPosition.X * Sin;
	return NewPoint;
}

void UUTHUDWidget_QuickStats::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	// Look to see if we should draw the ammo...

	DrawAngle = InUTHUDOwner->GetQuickStatsAngle();

	float DrawDistance = InUTHUDOwner->GetQuickStatsDistance();
	FName DisplayTag = InUTHUDOwner->GetQuickStatsType();
	DrawScale = InUTHUDOwner->GetQuickStatScaleOverride();

	HorizontalBackground.RenderOpacity = InUTHUDOwner->GetQuickStatsBackgroundAlpha();
	VerticalBackground.RenderOpacity = InUTHUDOwner->GetQuickStatsBackgroundAlpha();

	ForegroundOpacity = InUTHUDOwner->GetQuickStatsForegroundAlpha();

	if (DisplayTag != NAME_None && DisplayTag != CurrentLayoutTag)
	{
		for (int32 i=0; i < Layouts.Num(); i++)
		{
			if (Layouts[i].Tag == DisplayTag)
			{
				CurrentLayoutIndex = i;
				CurrentLayoutTag = DisplayTag;
				break;
			}
		}
	}

	Position = CalcDrawLocation( 1920.0f * DrawDistance, DrawAngle);
	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);

	AUTCharacter* CharOwner = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	AUTPlayerState* UTPlayerState = CharOwner != nullptr ? Cast<AUTPlayerState>(CharOwner->PlayerState) : nullptr;
	if (CharOwner && UTPlayerState)
	{
		if (UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->WeaponBobGlobalScaling > 0 && (InUTHUDOwner->GetQuickStatsBob()))
		{
			RenderPosition.X += 4.0f * CharOwner->CurrentWeaponBob.Y;
			RenderPosition.Y -= 4.0f * CharOwner->CurrentWeaponBob.Z;
		}
	
		AUTWeapon* Weap = CharOwner->GetWeapon();

		WeaponColor = Weap ? Weap->IconColor : FLinearColor::White;
		bHorizBorders = Layouts[CurrentLayoutIndex].bHorizontalBorder;

		// ----------------- Ammo
		AmmoInfo.Value = Weap ? Weap->Ammo : 0;
		AmmoInfo.bInfinite = Weap && !Weap->NeedsAmmoDisplay();
		AmmoInfo.IconColor = WeaponColor;

		UpdateStatScale(DeltaTime, AmmoInfo, Weap == LastWeapon);
		
		LastWeapon = Weap;
		if (Weap && !AmmoInfo.bInfinite && Weap->AmmoWarningAmount > 0)
		{
			float AmmoPerc = float(AmmoInfo.Value-1) / float(Weap->MaxAmmo);
			GetHighlightStrength(AmmoInfo, AmmoPerc, float(Weap->AmmoWarningAmount) / float(Weap->MaxAmmo));
		}
		else
		{
			AmmoInfo.HighlightStrength = 0.0f;
		}

		// ----------------- Health
		HealthInfo.Value = CharOwner->Health;
		HealthInfo.bInfinite = false;

		UpdateStatScale(DeltaTime, HealthInfo);

		float HealthPerc = float(HealthInfo.Value) / 100.0f;
		GetHighlightStrength(HealthInfo, HealthPerc, 0.5f);

		// ----------------- Armor / Inventory

		bool bHasShieldBelt = false;
		bool bHasBoots = false;
		
		ArmorInfo.Value = 0;
		BootsInfo.Value = 0;

		AUTTimedPowerup* CurrentPowerup = nullptr;

		for (TInventoryIterator<> It(CharOwner); It; ++It)
		{
			AUTArmor* Armor = Cast<AUTArmor>(*It);
			if (Armor != nullptr)
			{
				bHasShieldBelt |= (Armor->ArmorType == ArmorTypeName::ShieldBelt);
				ArmorInfo.Value += Armor->ArmorAmount;
			}

			AUTJumpBoots* Boots = Cast<AUTJumpBoots>(*It);
			if (Boots != nullptr)
			{
				bHasBoots = true;
				BootsInfo.Value = Boots->NumJumps;
			}

			if (CurrentPowerup == nullptr)
			{
				CurrentPowerup = Cast<AUTTimedPowerup>(*It);
			}
		}

		ArmorInfo.bInfinite = false;
		ArmorInfo.bAltIcon = bHasShieldBelt;
		
		UpdateStatScale(DeltaTime, ArmorInfo);

		float ArmorPerc = ArmorInfo.Value > 0 ? float(ArmorInfo.Value) / 100.0f : 1.0f;
		ArmorInfo.HighlightStrength = 0.0f;
		ArmorInfo.IconColor = bHasShieldBelt ? FLinearColor::Yellow : FLinearColor::White;

		if (bHasBoots)
		{
			UpdateStatScale(DeltaTime, BootsInfo);
			BootsInfo.HighlightStrength = 0.0;
		}
		else
		{
			BootsInfo.Value = 0;
		}

		if (CurrentPowerup == nullptr)
		{
			if (UTPlayerState->RemainingBoosts > 0)
			{
				PowerupInfo.bAltIcon = true;
				PowerupInfo.bUseLabel = true;
				PowerupInfo.Value = 1;

				if (PowerupInfo.Label.IsEmpty())
				{
					TArray<FString> Keys;
					UTPlayerOwner->ResolveKeybind(TEXT("ToggleTranslocator"), Keys, false, false);
					if (Keys.Num() > 0)
					{
						PowerupInfo.Label = FText::FromString(UTPlayerOwner->FixedupKeyname(Keys[0]));
					}
				}
			}
			else
			{
				PowerupInfo.Value = 0;
			}
		}
		else
		{
			PowerupInfo.bAltIcon = false;
			PowerupInfo.bUseLabel = false;
			PowerupInfo.Label = FText::GetEmpty();
			PowerupInfo.Value = int32(CurrentPowerup->TimeRemaining);

			PowerupIcon.Atlas = CurrentPowerup->HUDIcon.Texture;
			PowerupIcon.UVs.U = CurrentPowerup->HUDIcon.U;
			PowerupIcon.UVs.V = CurrentPowerup->HUDIcon.V;
			PowerupIcon.UVs.UL = CurrentPowerup->HUDIcon.UL;
			PowerupIcon.UVs.VL = CurrentPowerup->HUDIcon.VL;

			PowerupIcon.Position.Y = -22;
			PowerupIcon.RenderOffset = FVector2D(0.5f,0.5f);
			PowerupIcon.Size = FVector2D(PowerupIcon.UVs.UL * 1.07f, 35);

			if (PowerupInfo.Value <= 5)
			{
				PowerupInfo.HighlightStrength = float(PowerupInfo.Value) / 5.0f;
			}
		}

		if (UTPlayerState->CarriedObject != nullptr)
		{
			FlagInfo.Value = 1;
			FlagInfo.IconColor = UTPlayerState->CarriedObject->GetTeamNum() == 0 ? FLinearColor::Red : FLinearColor::Blue;
		}
		else
		{
			FlagInfo.Value = 0;
		}

	}
}

void UUTHUDWidget_QuickStats::UpdateStatScale(float DeltaTime, FStatInfo& Stat, bool bLookForChange)
{
	if (Stat.Value > Stat.LastValue && bLookForChange)
	{
		Stat.Scale = BOUNCE_SCALE;
	}
	else if (Stat.Scale > 1.0f)
	{
		Stat.Scale = FMath::Clamp<float>(Stat.Scale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
	}
	else
	{
		Stat.Scale = 1.0f;
	}

	Stat.LastValue = Stat.Value;
}

void UUTHUDWidget_QuickStats::Draw_Implementation(float DeltaTime)
{
	bool bFollowRotation = Layouts[CurrentLayoutIndex].bFollowRotation;
	DrawStat(bFollowRotation ? CalcRotOffset(Layouts[CurrentLayoutIndex].HealthOffset,DrawAngle) :	Layouts[CurrentLayoutIndex].HealthOffset, HealthInfo, HealthIcon);
	DrawStat(bFollowRotation ? CalcRotOffset(Layouts[CurrentLayoutIndex].ArmorOffset,DrawAngle) :	Layouts[CurrentLayoutIndex].ArmorOffset, ArmorInfo, ArmorInfo.bAltIcon ? ArmorIconWithShieldBelt : ArmorIcon);
	DrawStat(bFollowRotation ? CalcRotOffset(Layouts[CurrentLayoutIndex].AmmoOffset,DrawAngle) :	Layouts[CurrentLayoutIndex].AmmoOffset, AmmoInfo, AmmoIcon);

	if (FlagInfo.Value > 0)
	{
		DrawStat(bFollowRotation ? CalcRotOffset(Layouts[CurrentLayoutIndex].FlagOffset, DrawAngle) :		Layouts[CurrentLayoutIndex].FlagOffset, FlagInfo, FlagIcon);
	}

	if (BootsInfo.Value > 0)
	{
		DrawStat(bFollowRotation ? CalcRotOffset(Layouts[CurrentLayoutIndex].BootsOffset, DrawAngle) :		Layouts[CurrentLayoutIndex].BootsOffset, BootsInfo, BootsIcon);
	}

	if (PowerupInfo.Value > 0)
	{
		DrawStat(bFollowRotation ? CalcRotOffset(Layouts[CurrentLayoutIndex].PowerupOffset, DrawAngle) :	Layouts[CurrentLayoutIndex].AmmoOffset, PowerupInfo, PowerupInfo.bUseLabel ? BoostIcon : PowerupIcon);
	}

}

void UUTHUDWidget_QuickStats::DrawStat(FVector2D StatOffset, FStatInfo& Info, FHUDRenderObject_Texture Icon)
{
	if (bHorizBorders)
	{
		HorizontalBackground.RenderColor = Info.BackgroundColor;
		RenderObj_Texture(HorizontalBackground, StatOffset);

		if (Info.HighlightStrength> 0)
		{
			HorizontalHighlight.RenderColor = FLinearColor(0.82f,0.0f,0.0f,1.0f);
			FVector2D DrawOffset = StatOffset;

			float Height = HorizontalBackground.Size.Y * Info.HighlightStrength;
			HorizontalHighlight.Size.Y = Height;
			HorizontalHighlight.UVs.VL = Height;

			DrawOffset.Y -= (HorizontalBackground.Size.Y * 0.5f);
			RenderObj_Texture(HorizontalHighlight, DrawOffset);
		}

	}
	else
	{
		VerticalBackground.RenderColor = Info.BackgroundColor;
		RenderObj_Texture(VerticalBackground, StatOffset);
	}

	Icon.RenderScale = Info.Scale;
	Icon.RenderColor = Info.IconColor;
	Icon.RenderOpacity = ForegroundOpacity;
	RenderObj_Texture(Icon, StatOffset);

	if (!Info.bNoText)
	{
		if (Info.bInfinite)
		{
			RenderObj_Texture(InfiniteIcon, StatOffset);
		}
		else
		{
			TextTemplate.TextScale = Info.Scale;
			TextTemplate.Text = Info.bUseLabel ? Info.Label : FText::AsNumber(Info.Value);
			TextTemplate.RenderColor = Info.TextColor;
			TextTemplate.RenderOpacity = ForegroundOpacity;
			RenderObj_Text(TextTemplate, StatOffset);
		}
	}
}

void UUTHUDWidget_QuickStats::GetHighlightStrength(FStatInfo& Stat, float Perc, float WarnPerc)
{
	if (Perc <= WarnPerc)
	{
		Stat.HighlightStrength = 1.0f - (Perc / WarnPerc);
	}
	else
	{
		Stat.HighlightStrength = 0.0f;
	}
}

FLinearColor UUTHUDWidget_QuickStats::InterpColor(FLinearColor DestinationColor, float Delta)
{
	float AlphaSin = 1.0f - FMath::Abs<float>(FMath::Sin(Delta * 2.0f));
	
	float R = FMath::Lerp(DestinationColor.R, 0.0f, AlphaSin);
	float G = FMath::Lerp(DestinationColor.G, 0.0f, AlphaSin);
	float B = FMath::Lerp(DestinationColor.B, 0.0f, AlphaSin);
	return FLinearColor(R, G, B, 1.0f);
}

float UUTHUDWidget_QuickStats::GetDrawScaleOverride()
{
	return Super::GetDrawScaleOverride() * DrawScale;
}


