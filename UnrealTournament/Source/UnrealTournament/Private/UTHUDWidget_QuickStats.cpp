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
	ScreenPosition=FVector2D(0.5f, 0.55f);
	Origin=FVector2D(0.5f,0.0f);
	DesignedResolution=1080;
}

void UUTHUDWidget_QuickStats::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	PlayerArmor = 0;
	PlayerHealth = 0;
	PlayerAmmo = 0;
	PlayerBoots = 0;

	HealthText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_QuickStats::GetPlayerHealthText_Implementation);
	ArmorText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_QuickStats::GetPlayerArmorText_Implementation);
	AmmoText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_QuickStats::GetPlayerAmmoText_Implementation);
	BootsText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_QuickStats::GetPlayerBootsText_Implementation);
}

bool UUTHUDWidget_QuickStats::ShouldDraw_Implementation(bool bShowScores)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	return (!bShowScores && UTC && !UTC->IsDead());
}


void UUTHUDWidget_QuickStats::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	// Look to see if we should draw the ammo...

	AUTCharacter* UTC = Cast<AUTCharacter>(InUTHUDOwner->UTPlayerOwner->GetViewTarget());
	if (UTC && !UTC->IsDead())
	{
		AUTWeapon* Weap = UTC->GetWeapon();
		bool bDrawAmmo = false;
		PlayerAmmo = 0;
		if (Weap)
		{
			PlayerAmmo = Weap->Ammo;
			bDrawAmmo = Weap->NeedsAmmoDisplay();
		}

		LastWeapon = Weap;

		AmmoText.bHidden = !bDrawAmmo;
		AmmoIcon.bHidden = !bDrawAmmo;
		AmmoBackground.bHidden = !bDrawAmmo;
		AmmoBorder.bHidden = !bDrawAmmo;

		if (bDrawAmmo && Weap)
		{
			if (PlayerAmmo > LastAmmoAmount || Weap != LastWeapon)
			{
				// Set the icon scale
				AmmoIcon.RenderScale = BOUNCE_SCALE;
			}
			else if (AmmoIcon.RenderScale > 1.0f)
			{
				AmmoIcon.RenderScale = FMath::Clamp<float>(AmmoIcon.RenderScale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
			}

			float AmmoPerc = float(PlayerAmmo) / float(Weap->MaxAmmo);

			AmmoBackground.RenderColor = (AmmoPerc <= 0.25) ? GetFlashColor(GetWorld()->TimeSeconds) : FLinearColor(0.25f,0.25f,0.25f,1.0f);
			AmmoBackground.RenderOpacity = (AmmoPerc <= 0.25f) ? GetFlashOpacity(GetWorld()->TimeSeconds) : 0.5f;
		}

		LastWeapon = Weap;
		LastAmmoAmount = PlayerAmmo;

		PlayerHealth = UTC->Health;
		if (PlayerHealth > LastHealthAmount)
		{
			// Set the icon scale
			HealthIcon.RenderScale = BOUNCE_SCALE;
		}
		else if (HealthIcon.RenderScale > 1.0f)
		{
			HealthIcon.RenderScale = FMath::Clamp<float>(HealthIcon.RenderScale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
		}

		float HealthPerc = float(PlayerHealth) / 100.0f;
		HealthBackground.RenderColor = (HealthPerc < 0.33) ? GetFlashColor(GetWorld()->TimeSeconds) : FLinearColor(0.25f,0.25f,0.25f,0.5f);
		HealthBackground.RenderOpacity = (HealthPerc < 0.33) ? GetFlashOpacity(GetWorld()->TimeSeconds) : 0.5f;
		LastHealthAmount = PlayerHealth;

		bool bHasArmor = false;
		bool bHasBoots = false;


		PlayerArmor = 0;
		PlayerBoots = 0;

		for (TInventoryIterator<> It(UTC); It; ++It)
		{
			AUTArmor* Armor = Cast<AUTArmor>(*It);
			if (Armor != nullptr)
			{
				PlayerArmor += Armor->ArmorAmount;
				bHasArmor = true;
			}

			AUTJumpBoots* Boots = Cast<AUTJumpBoots>(*It);
			if (Boots != nullptr)
			{
				bHasBoots = true;
				PlayerBoots = Boots->NumJumps;
			}
		}

		if (bHasArmor)
		{
			ArmorBackground.bHidden = false;
			ArmorBorder.bHidden = false;
			ArmorIcon.bHidden = false;
			ArmorText.bHidden = false;

			if (PlayerArmor> LastArmorAmount)
			{
				// Set the icon scale
				ArmorIcon.RenderScale = BOUNCE_SCALE;
			}
			else if (ArmorIcon.RenderScale > 1.0f)
			{
				ArmorIcon.RenderScale = FMath::Clamp<float>(ArmorIcon.RenderScale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
			}

			float ArmorPerc = float(PlayerArmor) / 100.0f;
			ArmorBackground.RenderColor = (ArmorPerc <= 0.33f) ? GetFlashColor(GetWorld()->TimeSeconds) : FLinearColor(0.25f, 0.25f, 0.25f, 0.5f);
			ArmorBackground.RenderOpacity = (ArmorPerc <= 0.33f) ? GetFlashOpacity(GetWorld()->TimeSeconds) : 0.5f;
		}
		else
		{
			ArmorBackground.bHidden = true;
			ArmorBorder.bHidden = true;
			ArmorIcon.bHidden = true;
			ArmorText.bHidden = true;
		}
		LastArmorAmount = PlayerArmor;

		if (bHasBoots)
		{
			BootsBackground.bHidden = false;
			BootsBorder.bHidden = false;
			BootsIcon.bHidden = false;
			BootsText.bHidden = false;

			if (PlayerBoots> LastBootsAmount)
			{
				// Set the icon scale
				BootsIcon.RenderScale = BOUNCE_SCALE;
			}
			else if (BootsIcon.RenderScale > 1.0f)
			{
				BootsIcon.RenderScale = FMath::Clamp<float>(BootsIcon.RenderScale - BOUNCE_TIME * DeltaTime, 1.0f, BOUNCE_SCALE);
			}

			BootsBackground.RenderColor = (PlayerBoots < 2) ? GetFlashColor(GetWorld()->TimeSeconds) : FLinearColor(0.25f, 0.25f, 0.25f, 0.5f);
			BootsBackground.RenderOpacity = (PlayerBoots < 2) ? GetFlashOpacity(GetWorld()->TimeSeconds) : 0.5f;
		}
		else
		{
			BootsBackground.bHidden = true;
			BootsBorder.bHidden = true;
			BootsIcon.bHidden = true;
			BootsText.bHidden = true;
		}
		LastBootsAmount = PlayerBoots;

	}

	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);

	AUTCharacter* CharOwner = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	if (CharOwner)
	{
		RenderPosition.X += 3 * CharOwner->CurrentWeaponBob.Y;
		RenderPosition.Y -= 3 * CharOwner->CurrentWeaponBob.Z;
	}

}

FLinearColor UUTHUDWidget_QuickStats::GetFlashColor(float Delta)
{
	float AlphaSin = 1.0 - FMath::Abs<float>(FMath::Sin(Delta * 2));
	float Color = 0.5f - (0.5f * AlphaSin);
	float Red = 0.75f + (0.25f * AlphaSin);
	float Alpha = 0.5 + (0.25f * AlphaSin);
	return FLinearColor(Red, Color, Color, 1.0);
}

float UUTHUDWidget_QuickStats::GetFlashOpacity(float Delta)
{
	float AlphaSin = 1.0 - FMath::Abs<float>(FMath::Sin(Delta * 2));
	return 0.5 + (0.25f * AlphaSin);
}
void UUTHUDWidget_QuickStats::Draw_Implementation(float DeltaTime)
{
	PlayerArmor = 0;
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	if (UTC != NULL && !UTC->IsDead())
	{
		for (TInventoryIterator<> It(UTC); It; ++It)
		{
			AUTArmor* Armor = Cast<AUTArmor>(*It);
			if (Armor != NULL)
			{
				PlayerArmor += Armor->ArmorAmount;
			}
		}
	}

	Super::Draw_Implementation(DeltaTime);
}


FText UUTHUDWidget_QuickStats::GetPlayerHealthText_Implementation()
{
	return FText::AsNumber(PlayerHealth);
}

FText UUTHUDWidget_QuickStats::GetPlayerArmorText_Implementation()
{
	return FText::AsNumber(PlayerArmor);
}

FText UUTHUDWidget_QuickStats::GetPlayerAmmoText_Implementation()
{
	return FText::AsNumber(PlayerAmmo);
}

FText UUTHUDWidget_QuickStats::GetPlayerBootsText_Implementation()
{
	return FText::AsNumber(PlayerBoots);
}

