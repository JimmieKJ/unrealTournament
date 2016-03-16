// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_QuickStats.h"
#include "UTArmor.h"
#include "UTJumpBoots.h"
#include "UTWeapon.h"

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
}

bool UUTHUDWidget_QuickStats::ShouldDraw_Implementation(bool bShowScores)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	return (!bShowScores && UTC && !UTC->IsDead());
}

FVector2D UUTHUDWidget_QuickStats::CalcDrawLocation(float DistanceInPixels, float Angle)
{
	float Sin = 0;
	float Cos = 0;

	FMath::SinCos(&Sin,&Cos, FMath::DegreesToRadians(Angle));
	FVector2D NewPoint;

	NewPoint.X = DistanceInPixels * Sin;
	NewPoint.Y = -1.0f * DistanceInPixels * Cos;
	return NewPoint;
}

void UUTHUDWidget_QuickStats::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	// Look to see if we should draw the ammo...

	UUTProfileSettings* ProfileSettings = InUTHUDOwner->UTPlayerOwner->GetProfileSettings();
	float DrawAngle = ProfileSettings ? ProfileSettings->QuickStatsAngle : 180.0f;
	float DrawDistance = ProfileSettings ? ProfileSettings->QuickStatsDistance : 0.2f;
	FName DisplayTag = ProfileSettings ? ProfileSettings->QuickStatType : NAME_None;

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
	if (CharOwner)
	{
		RenderPosition.X += 4.0f * CharOwner->CurrentWeaponBob.Y;
		RenderPosition.Y -= 4.0f * CharOwner->CurrentWeaponBob.Z;
	
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(CharOwner->PlayerState);
		AUTWeapon* Weap = CharOwner->GetWeapon();

		bHorizBorders = Layouts[CurrentLayoutIndex].bHorizontalBorder;

		// ----------------- Ammo
		AmmoInfo.Value = Weap ? Weap->Ammo : 0;
		AmmoInfo.bInfinite = Weap && !Weap->NeedsAmmoDisplay();
		
		if (AmmoInfo.Value > AmmoInfo.LastValue && Weap == LastWeapon)
		{
			AmmoInfo.Scale = BOUNCE_SCALE;						
		}
		else if (AmmoInfo.Scale > 1.0f)
		{
			AmmoInfo.Scale = FMath::Clamp<float>(AmmoInfo.Scale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
		}
		else
		{
			AmmoInfo.Scale = 1.0f;
		}

		AmmoInfo.LastValue = AmmoInfo.Value;
		LastWeapon = Weap;
		float AmmoPerc = Weap ? (float(AmmoInfo.Value) / float(Weap->MaxAmmo)) : 1.0f;
		AmmoInfo.BackgroundColor = GetStatColor(AmmoPerc, 0.05f, 0.025f);

		// ----------------- Health
		HealthInfo.Value = CharOwner->Health;
		HealthInfo.bInfinite = false;
		
		if (HealthInfo.Value > HealthInfo.LastValue)
		{
			HealthInfo.Scale = BOUNCE_SCALE;						
		}
		else if (HealthInfo.Scale > 1.0f)
		{
			HealthInfo.Scale = FMath::Clamp<float>(HealthInfo.Scale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
		}
		else
		{
			HealthInfo.Scale = 1.0f;
		}
		HealthInfo.LastValue = HealthInfo.Value;
		float HealthPerc = AmmoInfo.bInfinite ? 1.0 : (HealthInfo.Value) / 100.0f;
		HealthInfo.BackgroundColor = GetStatColor(HealthPerc, 0.5f, 0.33f);

		// ----------------- Health

		bool bHasShieldBelt = false;
		ArmorInfo.Value = 0;
		for (TInventoryIterator<> It(CharOwner); It; ++It)
		{
			AUTArmor* Armor = Cast<AUTArmor>(*It);
			if (Armor != nullptr)
			{
				bHasShieldBelt |= (Armor->ArmorType == ArmorTypeName::ShieldBelt);
				ArmorInfo.Value += Armor->ArmorAmount;
			}
		}

		ArmorInfo.bInfinite = false;
		
		if (ArmorInfo.Value > ArmorInfo.LastValue)
		{
			ArmorInfo.Scale = BOUNCE_SCALE;						
		}
		else if (ArmorInfo.Scale > 1.0f)
		{
			ArmorInfo.Scale = FMath::Clamp<float>(ArmorInfo.Scale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
		}
		else
		{
			ArmorInfo.Scale = 1.0f;
		}

		ArmorInfo.LastValue = ArmorInfo.Value;
		float ArmorPerc = ArmorInfo.Value > 0 ? float(ArmorInfo.Value) / 100.0f : 1.0f;
		ArmorInfo.BackgroundColor = GetStatColor(ArmorPerc, 0.5f, 0.33f);

	}
}

void UUTHUDWidget_QuickStats::Draw_Implementation(float DeltaTime)
{
	DrawStat(Layouts[CurrentLayoutIndex].HealthOffset, HealthInfo, FLinearColor(0.4f, 0.95f, 0.48f, 1.0f), HealthIcon);
	DrawStat(Layouts[CurrentLayoutIndex].ArmorOffset, ArmorInfo, FLinearColor::White, ArmorInfo.bAltIcon ? ArmorIconWithShieldBelt : ArmorIcon);
	DrawStat(Layouts[CurrentLayoutIndex].AmmoOffset, AmmoInfo, FLinearColor::White, AmmoIcon);
}

void UUTHUDWidget_QuickStats::DrawStat(FVector2D StatOffset, FStatInfo& Info, FLinearColor TextColor, FHUDRenderObject_Texture Icon)
{
	if (bHorizBorders)
	{
		HorizontalBackground.RenderColor = Info.BackgroundColor;
		RenderObj_Texture(HorizontalBackground, StatOffset);
	}
	else
	{
		VerticalBackground.RenderColor = Info.BackgroundColor;
		RenderObj_Texture(VerticalBackground, StatOffset);
	}

	Icon.RenderScale = Info.Scale;
	RenderObj_Texture(Icon, StatOffset);


	if (Info.bInfinite)
	{
		RenderObj_Texture(InfiniteIcon, StatOffset);
	}
	else
	{
		TextTemplate.TextScale = Info.Scale;
		TextTemplate.Text = FText::AsNumber(Info.Value);
		TextTemplate.RenderColor = TextColor;
		RenderObj_Text(TextTemplate, StatOffset);
	}
}

FLinearColor UUTHUDWidget_QuickStats::GetStatColor(float Perc, float WarnPerc, float DangerPerc)
{
	if (Perc <= WarnPerc)
	{
		if (Perc <= DangerPerc)
		{
			// Shade of Red
			return InterpColor(FLinearColor(0.82f,0.0f,0.0f,1.0f), GetWorld()->TimeSeconds * 2.0f);
		}
		
		// Shade of Yellow
		return InterpColor(FLinearColor(0.8f,0.8f,0.22f,1.0f), GetWorld()->TimeSeconds);
	}
	// Black
	return FLinearColor::Black;
}

FLinearColor UUTHUDWidget_QuickStats::InterpColor(FLinearColor DestinationColor, float Delta)
{
	float AlphaSin = 1.0f - FMath::Abs<float>(FMath::Sin(Delta * 2.0f));
	
	float R = FMath::Lerp(DestinationColor.R, 0.0f, AlphaSin);
	float G = FMath::Lerp(DestinationColor.G, 0.0f, AlphaSin);
	float B = FMath::Lerp(DestinationColor.B, 0.0f, AlphaSin);
	return FLinearColor(R, G, B, 1.0f);
}

