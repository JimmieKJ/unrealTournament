// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_LinkGun.h"
#include "UTProj_LinkPlasma.h"
#include "UTTeamGameMode.h"
#include "UnrealNetwork.h"
#include "StatNames.h"
#include "UTSquadAI.h"
#include "UTWeaponStateFiring.h"
#include "Animation/AnimInstance.h"

AUTWeap_LinkGun::AUTWeap_LinkGun(const FObjectInitializer& OI)
: Super(OI)
{
	if (FiringState.Num() > 0)
	{
		FireInterval[1] = 0.12f;
		InstantHitInfo.AddZeroed();
		InstantHitInfo[0].Damage = 9;
		InstantHitInfo[0].TraceRange = 2200.0f;
	}
	if (AmmoCost.Num() < 2)
	{
		AmmoCost.SetNum(2);
	}
	DefaultGroup = 5;
	AmmoCost[0] = 1;
	AmmoCost[1] = 1;
	FOVOffset = FVector(0.6f, 1.f, 1.f);
	Ammo = 70;
	MaxAmmo = 200;
	AmmoWarningAmount = 25;
	AmmoDangerAmount = 10;

	HUDIcon = MakeCanvasIcon(HUDIcon.Texture, 453.0f, 467.0, 147.0f, 41.0f);

	BeamPulseInterval = 0.4f;
	BeamPulseMomentum = -220000.0f;
	BeamPulseAmmoCost = 4;
	PullWarmupTime = 0.15f;
	LinkPullDamage = 0;
	ReadyToPullColor = FLinearColor::White;

	bRecommendSuppressiveFire = true;

	KillStatsName = NAME_LinkKills;
	AltKillStatsName = NAME_LinkBeamKills;
	DeathStatsName = NAME_LinkDeaths;
	AltDeathStatsName = NAME_LinkBeamDeaths;
	HitsStatsName = NAME_LinkHits;
	ShotsStatsName = NAME_LinkShots;

	ScreenMaterialID = 2;
	SideScreenMaterialID = 1;
	FiringBeamKickbackY = 0.f;
	LinkPullKickbackY = 300.f;

	WeaponCustomizationTag = EpicWeaponCustomizationTags::LinkGun;
	WeaponSkinCustomizationTag = EpicWeaponSkinCustomizationTags::LinkGun;

	TutorialAnnouncements.Add(TEXT("PriLinkGun"));
	TutorialAnnouncements.Add(TEXT("SecLinkGun"));
}

void AUTWeap_LinkGun::AttachToOwner_Implementation()
{
	Super::AttachToOwner_Implementation();

	if (!IsRunningDedicatedServer() && Mesh != NULL && ScreenMaterialID < Mesh->GetNumMaterials())
	{
		ScreenMI = Mesh->CreateAndSetMaterialInstanceDynamic(ScreenMaterialID);
		ScreenTexture = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), 64, 64);
		ScreenTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		ScreenTexture->OnCanvasRenderTargetUpdate.AddDynamic(this, &AUTWeap_LinkGun::UpdateScreenTexture);
		ScreenMI->SetTextureParameterValue(FName(TEXT("ScreenTexture")), ScreenTexture);

		if (SideScreenMaterialID < Mesh->GetNumMaterials())
		{ 
			SideScreenMI = Mesh->CreateAndSetMaterialInstanceDynamic(SideScreenMaterialID);
		}
	}
}

void AUTWeap_LinkGun::UpdateScreenTexture(UCanvas* C, int32 Width, int32 Height)
{
	if (GetWorld()->TimeSeconds - LastClientKillTime < 2.5f && ScreenKillNotifyTexture != NULL)
	{
		C->SetDrawColor(FColor::White);
		C->DrawTile(ScreenKillNotifyTexture, 0.0f, 0.0f, float(Width), float(Height), 0.0f, 0.0f, ScreenKillNotifyTexture->GetSizeX(), ScreenKillNotifyTexture->GetSizeY());
	}
	else
	{
		FFontRenderInfo RenderInfo;
		RenderInfo.bClipText = true;
		RenderInfo.GlowInfo.bEnableGlow = true;
		RenderInfo.GlowInfo.GlowColor = FLinearColor(-0.75f, -0.75f, -0.75f, 1.0f);
		RenderInfo.GlowInfo.GlowOuterRadius.X = 0.45f;
		RenderInfo.GlowInfo.GlowOuterRadius.Y = 0.475f;
		RenderInfo.GlowInfo.GlowInnerRadius.X = 0.475f;
		RenderInfo.GlowInfo.GlowInnerRadius.Y = 0.5f;

		FString OverheatText = bIsInCoolDown ? TEXT("***") : FString::FromInt(int32(100.f*FMath::Clamp(OverheatFactor, 0.1f, 1.f)));
		float XL, YL;
		C->TextSize(ScreenFont, OverheatText, XL, YL);
		if (!WordWrapper.IsValid())
		{
			WordWrapper = MakeShareable(new FCanvasWordWrapper());
		}
		FLinearColor ScreenColor = (OverheatFactor <= (IsFiring() ? 0.5f : 0.f)) ? FLinearColor::Green : FLinearColor::Yellow;
		if (bIsInCoolDown)
		{
			ScreenColor = FLinearColor::Red;
		}
		if (ScreenMI)
		{
			ScreenMI->SetVectorParameterValue(FName(TEXT("Screen Color")), ScreenColor);
			ScreenMI->SetVectorParameterValue(FName(TEXT("Screen Low Color")), ScreenColor);
		}
		if (SideScreenMI)
		{
			SideScreenMI->SetVectorParameterValue(FName(TEXT("Screen Color")), ScreenColor);
			SideScreenMI->SetVectorParameterValue(FName(TEXT("Screen Low Color")), ScreenColor);
		}
		C->SetDrawColor(ScreenColor.ToFColor(true));
		FUTCanvasTextItem Item(FVector2D(Width / 2 - XL * 0.5f, Height / 2 - YL * 0.5f), FText::FromString(OverheatText), ScreenFont, ScreenColor, WordWrapper);
		Item.FontRenderInfo = RenderInfo;
		C->DrawItem(Item);
	}
}

void AUTWeap_LinkGun::FireShot()
{
	if (!bIsInCoolDown)
	{
		Super::FireShot();
	}
}

AUTProjectile* AUTWeap_LinkGun::FireProjectile()
{
	AUTProj_LinkPlasma* LinkProj = Cast<AUTProj_LinkPlasma>(Super::FireProjectile());
	if (LinkProj != NULL)
	{
		LastFiredPlasmaTime = GetWorld()->GetTimeSeconds();
	}
	
	return LinkProj;
}

UAnimMontage* AUTWeap_LinkGun::GetFiringAnim(uint8 FireMode, bool bOnHands) const
{
	if (FireMode == 0 && bIsInCoolDown && OverheatAnim && !bOnHands)
	{
		return OverheatAnim;
	}
	else
	{
		return Super::GetFiringAnim(FireMode, bOnHands);
	}
}

void AUTWeap_LinkGun::PlayWeaponAnim(UAnimMontage* WeaponAnim, UAnimMontage* HandsAnim, float RateOverride)
{
	// give pull anim priority
	if (WeaponAnim == PulseAnim || PulseAnim == NULL || Mesh->GetAnimInstance() == NULL || !Mesh->GetAnimInstance()->Montage_IsPlaying(PulseAnim))
	{
		Super::PlayWeaponAnim(WeaponAnim, HandsAnim, RateOverride);
	}
}

bool AUTWeap_LinkGun::IsLinkPulsing()
{
	return (GetWorld()->GetTimeSeconds() - LastBeamPulseTime < BeamPulseInterval);
}

void AUTWeap_LinkGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsInCoolDown)
	{
		OverheatFactor -= 2.f * DeltaTime;
		if (OverheatFactor <= 0.f)
		{
			OverheatFactor = 0.f;
			bIsInCoolDown = false;
			if (UTOwner)
			{
				UTOwner->SetAmbientSound(OverheatSound, true);
				UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
				if ((AnimInstance != NULL) && (EndOverheatAnimSection != NAME_None))
				{
					AnimInstance->Montage_JumpToSection(EndOverheatAnimSection, OverheatAnim);
				}
			}
		}
		else if (UTOwner && OverheatSound)
		{
			UTOwner->SetAmbientSound(OverheatSound, false);
			UTOwner->ChangeAmbientSoundPitch(OverheatSound, 0.5f + OverheatFactor);
		}
	}
	else
	{
		OverheatFactor = (UTOwner && IsFiring() && (CurrentFireMode == 0) && (UTOwner->GetFireRateMultiplier() <= 1.f)) ? OverheatFactor + 0.42f*DeltaTime : FMath::Max(0.f, OverheatFactor - 2.2f * FMath::Max(0.f, DeltaTime - FMath::Max(0.f, 0.3f + LastFiredPlasmaTime - GetWorld()->GetTimeSeconds())));
		bIsInCoolDown = (OverheatFactor > 1.f);

	}
	if (ScreenTexture != NULL && Mesh->IsRegistered() && GetWorld()->TimeSeconds - Mesh->LastRenderTime < 0.1f)
	{
		ScreenTexture->FastUpdateResource();
	}

	if (MuzzleFlash.IsValidIndex(1) && MuzzleFlash[1] != NULL)
	{
		static FName NAME_PulseScale(TEXT("PulseScale"));
		float NewScale = 1.0f + FMath::Max<float>(0.0f, 1.0f - (GetWorld()->TimeSeconds - LastBeamPulseTime) / 0.35f);
		MuzzleFlash[1]->SetVectorParameter(NAME_PulseScale, FVector(NewScale, NewScale, NewScale));
	}

	if (UTOwner && IsFiring())
	{
		if ((Role == ROLE_Authority) && FireLoopingSound.IsValidIndex(CurrentFireMode) && FireLoopingSound[CurrentFireMode] != NULL && !IsLinkPulsing())
		{
			if (!bLinkBeamImpacting)
			{
				UTOwner->ChangeAmbientSoundPitch(FireLoopingSound[CurrentFireMode], 0.7f);
			}
			else if (bLinkCausingDamage)
			{
				UTOwner->ChangeAmbientSoundPitch(FireLoopingSound[CurrentFireMode], bReadyToPull ? 2.f :  1.7f);

			}
			else
			{
				UTOwner->ChangeAmbientSoundPitch(FireLoopingSound[CurrentFireMode], 1.f);
			}
		}
	}
	if (IsLinkPulsing())
	{
		// update link pull pulse beam endpoint
		const FVector SpawnLocation = GetFireStartLoc();
		const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
		const FVector FireDir = SpawnRotation.Vector();
		PulseLoc = (PulseTarget && !PulseTarget->IsPendingKillPending()) ? PulseTarget->GetActorLocation() : SpawnLocation + GetBaseFireRotation().RotateVector(MissedPulseOffset) + 100.f*FireDir;
/*
		// don't allow beam to go behind player
		FVector PulseDir = PulseLoc - SpawnLocation;
		float PulseDist = PulseDir.Size();
		PulseDir = (PulseDist > 0.f) ? PulseDir / PulseDist : PulseDir;
		if ((PulseDir | FireDir) < 0.7f)
		{
			PulseDir = PulseDir - FireDir * ((PulseDir | FireDir) - 0.7f);
			PulseDir = PulseDir.GetSafeNormal() * PulseDist;
			PulseLoc = PulseDir + SpawnLocation;
		}

		// make sure beam doesn't clip through geometry
		FHitResult Hit;
		HitScanTrace(SpawnLocation, PulseLoc, 0.f, Hit, 0.f);
		if (Hit.Time < 1.f)
		{
			PulseLoc = Hit.Location;
		}*/
	}
}

void AUTWeap_LinkGun::StartLinkPull()
{
	bReadyToPull = false;
	PulseTarget = nullptr;
	if (UTOwner && CurrentLinkedTarget && UTOwner->IsLocallyControlled())
	{
		LastBeamPulseTime = GetWorld()->TimeSeconds;
		PulseTarget = CurrentLinkedTarget;
		PulseLoc = PulseTarget->GetActorLocation();
		UTOwner->TargetEyeOffset.Y = LinkPullKickbackY;
		ServerSetPulseTarget(CurrentLinkedTarget);

		MuzzleFlash[FiringState.Num()]->SetTemplate(PulseSuccessEffect);
		MuzzleFlash[FiringState.Num()]->SetActorParameter(FName(TEXT("Player")), CurrentLinkedTarget);
		PlayWeaponAnim(PulseAnim, PulseAnimHands);
	}
	CurrentLinkedTarget = nullptr;
	LinkStartTime = -100.f;
}

bool AUTWeap_LinkGun::ServerSetPulseTarget_Validate(AActor* InTarget)
{
	return true;
}
void AUTWeap_LinkGun::ServerSetPulseTarget_Implementation(AActor* InTarget)
{
	if (!UTOwner || !UTOwner->Controller || !InTarget)
	{
		return;
	}
	AActor* ClientPulseTarget = InTarget;
	FHitResult Hit;
	Super::FireInstantHit(false, &Hit);
	FVector PulseStart = UTOwner->GetActorLocation();

	PulseTarget = Hit.Actor.Get();
	// try CSHD result if it's reasonable
	if (PulseTarget == NULL && ClientPulseTarget != NULL)
	{
		const FVector SpawnLocation = GetFireStartLoc();
		const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
		const FVector FireDir = SpawnRotation.Vector();
		const FVector EndTrace = SpawnLocation + FireDir * InstantHitInfo[CurrentFireMode].TraceRange;
		if (FMath::PointDistToSegment(ClientPulseTarget->GetActorLocation(), SpawnLocation, EndTrace) < 100.0f + ClientPulseTarget->GetSimpleCollisionRadius() && !GetWorld()->LineTraceTestByChannel(SpawnLocation, EndTrace, COLLISION_TRACE_WEAPONNOCHARACTER, FCollisionQueryParams(NAME_None, true)))
		{
			PulseTarget = ClientPulseTarget;
		}
	}
	if (PulseTarget != NULL)
	{
		// use owner to target direction instead of exactly the weapon orientation so that shooting below center doesn't cause the pull to send them over the shooter's head
		const FVector Dir = (PulseTarget->GetActorLocation() - PulseStart).GetSafeNormal();
		PulseLoc = PulseTarget->GetActorLocation();
		PulseTarget->TakeDamage(LinkPullDamage, FUTPointDamageEvent(0.0f, Hit, Dir, BeamPulseDamageType, BeamPulseMomentum * Dir), UTOwner->Controller, this);
		UUTGameplayStatics::UTPlaySound(GetWorld(), PullSucceeded, UTOwner, SRT_All, false, FVector::ZeroVector, Cast<AUTPlayerController>(PulseTarget->GetInstigatorController()), UTOwner, true, SAT_WeaponFire);

		UTOwner->SetFlashExtra(UTOwner->FlashExtra + 1, CurrentFireMode);
		if (Role == ROLE_Authority)
		{
			AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (!GameMode || GameMode->bAmmoIsLimited || (Ammo > 11))
			{
				AddAmmo(-BeamPulseAmmoCost);
			}
		}
		PlayWeaponAnim(PulseAnim, PulseAnimHands);
		// use an extra muzzle flash slot at the end for the pulse effect
		if (MuzzleFlash.IsValidIndex(FiringState.Num()) && MuzzleFlash[FiringState.Num()] != NULL)
		{
			if (PulseTarget != NULL)
			{
				MuzzleFlash[FiringState.Num()]->SetTemplate(PulseSuccessEffect);
				MuzzleFlash[FiringState.Num()]->SetActorParameter(FName(TEXT("Player")), PulseTarget);
			}
			else
			{
				MuzzleFlash[FiringState.Num()]->SetTemplate(PulseFailEffect);
			}
			MuzzleFlash[FiringState.Num()]->ActivateSystem();
		}
		LastBeamPulseTime = GetWorld()->TimeSeconds;
	}
	else
	{
		const FVector SpawnLocation = GetFireStartLoc();
		const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
		const FVector FireDir = SpawnRotation.Vector();
		PulseLoc = SpawnLocation + GetBaseFireRotation().RotateVector(MissedPulseOffset) + 100.f*FireDir;
		UUTGameplayStatics::UTPlaySound(GetWorld(), PullFailed, UTOwner, SRT_All, false, FVector::ZeroVector, nullptr, UTOwner, true, SAT_WeaponFoley);
	}
	CurrentLinkedTarget = nullptr;
	LinkStartTime = -100.f;
}


void AUTWeap_LinkGun::StateChanged()
{
	Super::StateChanged();

	// set AI timer for beam pulse
	static FName NAME_CheckBotPulseFire(TEXT("CheckBotPulseFire"));
	if (CurrentFireMode == 1 && Cast<UUTWeaponStateFiring>(CurrentState) != NULL && Cast<AUTBot>(UTOwner->Controller) != NULL)
	{
		SetTimerUFunc(this, NAME_CheckBotPulseFire, 0.2f, true);
	}
	else
	{
		ClearTimerUFunc(this, NAME_CheckBotPulseFire);
	}
}

void AUTWeap_LinkGun::CheckBotPulseFire()
{
	if (UTOwner != NULL && CurrentFireMode == 1 && InstantHitInfo.IsValidIndex(1))
	{
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if ( B != NULL && B->WeaponProficiencyCheck() && B->GetEnemy() != NULL && B->GetTarget() == B->GetEnemy() &&
			(B->IsCharging() || B->GetSquad()->MustKeepEnemy(B, B->GetEnemy()) || B->RelativeStrength(B->GetEnemy()) < 0.0f) )
		{
			bool bTryPulse = FMath::FRand() < (B->IsFavoriteWeapon(GetClass()) ? 0.1f : 0.05f);
			if (bTryPulse)
			{
				// if bot has good reflexes only pulse if enemy is being hit
				if (FMath::FRand() < 0.07f * B->Skill + B->Personality.ReactionTime)
				{
					const FVector SpawnLocation = GetFireStartLoc();
					const FVector EndTrace = SpawnLocation + GetAdjustedAim(SpawnLocation).Vector() * InstantHitInfo[1].TraceRange;
					FHitResult Hit;
					HitScanTrace(SpawnLocation, EndTrace, InstantHitInfo[1].TraceHalfSize, Hit, 0.0f);
					bTryPulse = Hit.Actor.Get() == B->GetEnemy();
				}
				if (bTryPulse)
				{
					StartFire(0);
					StopFire(0);
				}
			}
		}
	}
}

void AUTWeap_LinkGun::FiringExtraUpdated_Implementation(uint8 NewFlashExtra, uint8 InFireMode)
{
	if (NewFlashExtra > 0 && InFireMode == 1)
	{
		LastBeamPulseTime = GetWorld()->TimeSeconds;
		// use an extra muzzle flash slot at the end for the pulse effect
		if (MuzzleFlash.IsValidIndex(FiringState.Num()) && MuzzleFlash[FiringState.Num()] != NULL)
		{
			AActor* GuessTarget = PulseTarget;
			if (GuessTarget == NULL && UTOwner != NULL && Role < ROLE_Authority)
			{
				TArray<FOverlapResult> Hits;
				GetWorld()->OverlapMultiByChannel(Hits, UTOwner->FlashLocation, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeSphere(10.0f), FCollisionQueryParams(NAME_None, true, UTOwner));
				for (const FOverlapResult& Hit : Hits)
				{
					if (Cast<APawn>(Hit.Actor.Get()) != NULL)
					{
						GuessTarget = Hit.Actor.Get();
					}
				}
			}
			if (GuessTarget != NULL)
			{
				MuzzleFlash[FiringState.Num()]->SetTemplate(PulseSuccessEffect);
				MuzzleFlash[FiringState.Num()]->SetActorParameter(FName(TEXT("Player")), GuessTarget);
			}
			else
			{
				MuzzleFlash[FiringState.Num()]->SetTemplate(PulseFailEffect);
			}
		}
		PlayWeaponAnim(PulseAnim, PulseAnimHands);
	}
}

void AUTWeap_LinkGun::DrawWeaponCrosshair_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	Super::DrawWeaponCrosshair_Implementation(WeaponHudWidget, RenderDelta);

	if ((OverheatFactor > 0.f) && WeaponHudWidget && WeaponHudWidget->UTHUDOwner)
	{
		float Width = 150.f;
		float Height = 21.f;
		float WidthScale = WeaponHudWidget->GetRenderScale();
		float HeightScale = (bIsInCoolDown ? 1.f : 0.5f) * WidthScale;
		WidthScale *= (bIsInCoolDown ? 0.75f : 0.5f);
	//	WeaponHudWidget->DrawTexture(WeaponHudWidget->UTHUDOwner->HUDAtlas, 0.f, 96.f, Scale*Width, Scale*Height, 127, 671, Width, Height, 0.7f, FLinearColor::White, FVector2D(0.5f, 0.5f));
		FLinearColor ChargeColor = FLinearColor::White;
		float ChargePct = FMath::Clamp(OverheatFactor, 0.f, 1.f);
		WeaponHudWidget->DrawTexture(WeaponHudWidget->UTHUDOwner->HUDAtlas, 0.f, 32.f, WidthScale*Width*ChargePct, HeightScale*Height, 127, 641, Width, Height, bIsInCoolDown ? OverheatFactor : 0.7f, FLinearColor::White, FVector2D(0.5f, 0.5f));
		if (bIsInCoolDown)
		{
			WeaponHudWidget->DrawText(NSLOCTEXT("LinkGun", "Overheat", "OVERHEAT"), 0.f, 28.f, WeaponHudWidget->UTHUDOwner->TinyFont, 1.f, FMath::Min(3.f*OverheatFactor, 1.f), FLinearColor::Yellow, ETextHorzPos::Center, ETextVertPos::Center);
		}
		WeaponHudWidget->DrawTexture(WeaponHudWidget->UTHUDOwner->HUDAtlas, 0.f, 32.f, WidthScale*Width, HeightScale*Height, 127, 612, Width, Height, 1.f, FLinearColor::White, FVector2D(0.5f, 0.5f));
	}
	if (bReadyToPull  && WeaponHudWidget && WeaponHudWidget->UTHUDOwner)
	{
		float CircleSize = 76.f;
		float CrosshairScale = GetCrosshairScale(WeaponHudWidget->UTHUDOwner);
		WeaponHudWidget->DrawTexture(WeaponHudWidget->UTHUDOwner->HUDAtlas, 0, 0, 0.75f*CircleSize * CrosshairScale, 0.75f*CircleSize * CrosshairScale, 98, 936, CircleSize, CircleSize, 1.f, FLinearColor::Red, FVector2D(0.5f, 0.5f));
	}
}
