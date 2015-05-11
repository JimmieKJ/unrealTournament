// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTJumpBoots.h"
#include "UTArmor.h"

UUTHUDWidget_Paperdoll::UUTHUDWidget_Paperdoll(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));

	Position=FVector2D(5.0f, -5.0f);
	Size=FVector2D(205.0f,111.0f);
	ScreenPosition=FVector2D(0.0f, 1.0f);
	Origin=FVector2D(0.0f,1.0f);
}

void UUTHUDWidget_Paperdoll::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
	HealthText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_Paperdoll::GetPlayerHealth_Implementation);
	ArmorText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_Paperdoll::GetPlayerArmor_Implementation);
	LastHealth = 100;
	LastArmor = 0;
	ArmorFlashTimer = 0.0f;
	HealthFlashTimer = 0.0f;
}

FText UUTHUDWidget_Paperdoll::GetPlayerHealth_Implementation()
{
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	return (UTC != NULL && !UTC->IsDead()) ? FText::AsNumber(UTC->Health) : FText::AsNumber(0);
}

FText UUTHUDWidget_Paperdoll::GetPlayerArmor_Implementation()
{
	return FText::AsNumber(PlayerArmor);
}

void UUTHUDWidget_Paperdoll::ProcessArmor()
{
	PlayerArmor = 0;
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	bool bHasShieldBelt = false;
	bool bHasThighPads = false;
	bool bHasChest = false;
	bool bHasHelmet = false;
	bool bHasJumpBoots = false;
	if (UTHUDOwner->UTPlayerOwner->PlayerState && UTHUDOwner->UTPlayerOwner->PlayerState->bOnlySpectator && (UTHUDOwner->GetNetMode() != NM_Standalone))
	{
		bHasChest = (UTC->ArmorAmount > 0);
		bHasThighPads = (UTC->ArmorAmount > 100);
		PlayerArmor = UTC->ArmorAmount;
	}
	else if (UTC != NULL && !UTC->IsDead())
	{
		for (TInventoryIterator<> It(UTC); It; ++It)
		{
			AUTArmor* Armor = Cast<AUTArmor>(*It);
			if (Armor != NULL)
			{
				PlayerArmor += Armor->ArmorAmount;

				if (Armor->ArmorType == FName(TEXT("ShieldBelt"))) bHasShieldBelt = true;
				if (Armor->ArmorType == FName(TEXT("ThighPads"))) bHasThighPads = true;
				if (Armor->ArmorType == FName(TEXT("FlakVest"))) bHasChest = true;
				if (Armor->ArmorType == FName(TEXT("Helmet"))) bHasHelmet = true;
			}
			else if (Cast<AUTJumpBoots>(*It) != NULL) bHasJumpBoots = true;
		} 
	}
	PaperDoll_ShieldBeltOverlay.bHidden = !bHasShieldBelt;
	PaperDoll_ChestArmorOverlay.bHidden = !bHasChest;
	PaperDoll_HelmetOverlay.bHidden = !bHasHelmet;
	PaperDoll_ThighPadArmorOverlay.bHidden = !bHasThighPads;
	PaperDoll_BootsOverlay.bHidden = !bHasJumpBoots;
}

bool UUTHUDWidget_Paperdoll::ShouldDraw_Implementation(bool bShowScores)
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	return ((GS == NULL || !GS->HasMatchEnded()) && Super::ShouldDraw_Implementation(bShowScores));
}

void UUTHUDWidget_Paperdoll::Draw_Implementation(float DeltaTime)
{
	ProcessArmor();

	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	UUTHUDWidget_Paperdoll* DefObj = GetClass()->GetDefaultObject<UUTHUDWidget_Paperdoll>();

	if (UTC != NULL && !UTC->IsDead())
	{
		if (PlayerArmor != LastArmor)
		{
			ArmorText.RenderColor = (PlayerArmor > LastArmor) ? ArmorPositiveFlashColor : ArmorNegativeFlashColor;
			ArmorFlashTimer = ArmorFlashTime;		
			LastArmor = PlayerArmor;
			ArmorText.TextScale = 2.f;
		}
		else if (ArmorFlashTimer > 0.f)
		{
			ArmorFlashTimer = ArmorFlashTimer - DeltaTime;
			if (ArmorFlashTimer < 0.5f*ArmorFlashTime)
			{
				ArmorText.RenderColor = FMath::CInterpTo(ArmorText.RenderColor, DefObj->ArmorText.RenderColor, DeltaTime, (1.f / (ArmorFlashTime > 0.f ? 2.f*ArmorFlashTime : 1.f)));
			}
			ArmorText.TextScale = 1.f + ArmorFlashTimer / ArmorFlashTime;
		}
		else
		{
			ArmorText.RenderColor = DefObj->ArmorText.RenderColor; 
			ArmorText.TextScale = 1.f;
		}

		if (UTC->Health != LastHealth)
		{
			HealthText.RenderColor = (UTC->Health > LastHealth) ? HealthPositiveFlashColor : HealthNegativeFlashColor;
			HealthFlashTimer = HealthFlashTime;		
			LastHealth = UTC->Health;
			HealthText.TextScale = 2.f;

		}
		else if (HealthFlashTimer > 0.f)
		{
			HealthFlashTimer = HealthFlashTimer - DeltaTime;
			if (HealthFlashTimer < 0.5f*HealthFlashTime)
			{
				HealthText.RenderColor = FMath::CInterpTo(HealthText.RenderColor, DefObj->HealthText.RenderColor, DeltaTime, (1.f / (HealthFlashTime > 0.f ? 2.f*HealthFlashTime : 1.f)));
			}
			HealthText.TextScale = 1.f + HealthFlashTimer / HealthFlashTime;
		}
		else
		{
			HealthText.RenderColor = DefObj->HealthText.RenderColor;
			HealthText.TextScale = 1.f;
		}
	}

	Super::Draw_Implementation(DeltaTime);
}

