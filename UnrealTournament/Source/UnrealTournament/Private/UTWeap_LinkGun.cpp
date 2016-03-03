// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_LinkGun.h"
#include "UTProj_BioShot.h"
#include "UTProj_LinkPlasma.h"
#include "UTTeamGameMode.h"
#include "UnrealNetwork.h"
#include "StatNames.h"
#include "UTSquadAI.h"
#include "UTWeaponStateFiring.h"

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

	HUDIcon = MakeCanvasIcon(HUDIcon.Texture, 453.0f, 467.0, 147.0f, 41.0f);

	LinkBreakDelay = 0.5f;
	LinkFlexibility = 0.64f;
	LinkDistanceScaling = 1.5f;

	BeamPulseInterval = 0.7f;
	BeamPulseMomentum = -220000.0f;
	BeamPulseAmmoCost = 8;
	LinkPullDamage = 32;

	PerLinkDamageScalingPrimary = 1.f;
	PerLinkDamageScalingSecondary = 1.25f;
	PerLinkDistanceScalingSecondary = 0.2f;

	LinkedBio = NULL;
	bRecommendSuppressiveFire = true;

	KillStatsName = NAME_LinkKills;
	AltKillStatsName = NAME_LinkBeamKills;
	DeathStatsName = NAME_LinkDeaths;
	AltDeathStatsName = NAME_LinkBeamDeaths;
	HitsStatsName = NAME_LinkHits;
	ShotsStatsName = NAME_LinkShots;

	ScreenMaterialID = 2;
	LastClientKillTime = -100000.0f;
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

		bool bInfiniteAmmo = true;
		for (int32 Cost : AmmoCost)
		{
			if (Cost > 0)
			{
				bInfiniteAmmo = false;
				break;
			}
		}
		FString AmmoText = bInfiniteAmmo ? TEXT("--") : FString::FromInt(Ammo);
		float XL, YL;
		C->TextSize(ScreenFont, AmmoText, XL, YL);
		if (!WordWrapper.IsValid())
		{
			WordWrapper = MakeShareable(new FCanvasWordWrapper());
		}
		FUTCanvasTextItem Item(FVector2D(Width / 2 - XL * 0.5f, Height / 2 - YL * 0.5f), FText::FromString(AmmoText), ScreenFont, (Ammo <= 5) ? FLinearColor::Red : FLinearColor::White, WordWrapper);
		Item.FontRenderInfo = RenderInfo;
		C->DrawItem(Item);
	}
}

AUTProjectile* AUTWeap_LinkGun::FireProjectile()
{
	AUTProj_LinkPlasma* LinkProj = Cast<AUTProj_LinkPlasma>(Super::FireProjectile());
	if (LinkProj != NULL)
	{
		LinkProj->SetLinks(Links);
		LinkProj->DamageParams.BaseDamage += LinkProj->DamageParams.BaseDamage * PerLinkDamageScalingPrimary * Links;
	}
	
	return LinkProj;
}

void AUTWeap_LinkGun::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	AController* PulseInstigator = UTOwner->Controller;
	FVector PulseStart = UTOwner->GetActorLocation();

	FHitResult TempHit;
	if (OutHit == NULL)
	{
		OutHit = &TempHit;
	}

	const FVector SpawnLocation = GetFireStartLoc();

	FHitResult Hit;
	if (LinkTarget != nullptr && IsLinkValid())
	{
		Hit.Location = LinkTarget->GetActorLocation();
		Hit.Actor = LinkTarget;
		// don't need to bother with damage since linked
		if (Role == ROLE_Authority)
		{
			UTOwner->SetFlashLocation(Hit.Location, 10);
		}
		if (OutHit != NULL)
		{
			*OutHit = Hit;
		}
	}
	else
	{
		// TODO: Reset Link here since Link wasn't valid?
		LinkBreakTime = -1.f;
		ClearLinksTo();

		Super::FireInstantHit(bDealDamage, OutHit);
	}

	if (bPendingBeamPulse)
	{
		if (PulseInstigator != NULL)
		{
			PulseTarget = (LinkTarget != NULL) ? LinkTarget : OutHit->Actor.Get();
			if (PulseTarget != NULL)
			{
				// use owner to target direction instead of exactly the weapon orientation so that shooting below center doesn't cause the pull to send them over the shooter's head
				const FVector Dir = (PulseTarget->GetActorLocation() - PulseStart).GetSafeNormal();
				PulseTarget->TakeDamage(LinkPullDamage, FUTPointDamageEvent(0.0f, *OutHit, Dir, BeamPulseDamageType, BeamPulseMomentum * Dir), PulseInstigator, this);
				PulseLoc = PulseTarget->GetActorLocation();
			}
			else if (OutHit->bBlockingHit)
			{
				PulseLoc = OutHit->Location;
			}
			else
			{
				const FVector SpawnLocation = GetFireStartLoc();
				const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
				const FVector FireDir = SpawnRotation.Vector();
				PulseLoc = SpawnLocation + FireDir * InstantHitInfo[CurrentFireMode].TraceRange;
			}
		}
		if (UTOwner != NULL)
		{
			UTOwner->SetFlashExtra(UTOwner->FlashExtra + 1, CurrentFireMode);
		}
		if (Role == ROLE_Authority)
		{
			AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (!GameMode || GameMode->bAmmoIsLimited || (Ammo > 9))
			{
				AddAmmo(-BeamPulseAmmoCost);
			}
		}
		PlayWeaponAnim(PulseAnim, PulseAnimHands);
		// use an extra muzzle flash slot at the end for the pulse effect
		if (MuzzleFlash.IsValidIndex(FiringState.Num()) && MuzzleFlash[FiringState.Num()] != NULL)
		{
			MuzzleFlash[FiringState.Num()]->ActivateSystem();
		}
		LastBeamPulseTime = GetWorld()->TimeSeconds;
		bPendingBeamPulse = false;
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
		if ((Role == ROLE_Authority) && FireLoopingSound.IsValidIndex(CurrentFireMode) && FireLoopingSound[CurrentFireMode] != NULL)
		{
			UTOwner->ChangeAmbientSoundPitch(FireLoopingSound[CurrentFireMode], bLinkCausingDamage ? 2.f : 1.f);
		}
		if (IsLinkPulsing())
		{
			// update link pull pulse beam endpoint
			const FVector SpawnLocation = GetFireStartLoc();
			const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
			const FVector FireDir = SpawnRotation.Vector();
			PulseLoc = (PulseTarget && !PulseTarget->IsPendingKillPending()) ? PulseTarget->GetActorLocation() : PulseLoc - 5.f * DeltaTime * (PulseLoc - (SpawnLocation + 80.f*FireDir));

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
			}
		}
	}
}

bool AUTWeap_LinkGun::IsLinkValid_Implementation()
{
	const FVector Dir = GetAdjustedAim(GetFireStartLoc()).Vector();
	const FVector TargetDir = (LinkTarget->GetActorLocation() - GetUTOwner()->GetActorLocation()).GetSafeNormal();

	// do a trace to prevent link through objects
	FCollisionQueryParams TraceParams;
	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, GetFireStartLoc(), LinkTarget->GetActorLocation(), ECC_Visibility, TraceParams))
	{
		if (HitResult.GetActor() != LinkTarget)
		{
			// blocked by other
			return false;
		}
	}

	// distance check
	float linkMaxDistance = (InstantHitInfo[GetCurrentFireMode()].TraceRange + (InstantHitInfo[GetCurrentFireMode()].TraceRange * PerLinkDistanceScalingSecondary * Links)) * LinkDistanceScaling;
	if (FVector::Dist(Dir, TargetDir) > linkMaxDistance)
	{
		// exceeded distance
		return false;
	}
	return (FVector::DotProduct(Dir, TargetDir) > LinkFlexibility);
}

void AUTWeap_LinkGun::ConsumeAmmo(uint8 FireModeNum)
{
	LinkedConsumeAmmo(FireModeNum);
	Super::ConsumeAmmo(FireModeNum);
}

void AUTWeap_LinkGun::LinkedConsumeAmmo(int32 Mode)
{
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter* C = Cast<AUTCharacter>(*It);
		if (C != NULL)
		{
			AUTWeap_LinkGun* OtherLinkGun = Cast<AUTWeap_LinkGun>(C->GetWeapon());
			if (OtherLinkGun != NULL && OtherLinkGun->LinkedTo(this))
			{
				OtherLinkGun->ConsumeAmmo(Mode);
			}
		}
	}
}

bool AUTWeap_LinkGun::LinkedTo(AUTWeap_LinkGun* L)
{
	AUTCharacter* LinkCharacter = Cast<AUTCharacter>(LinkTarget);
	if (LinkCharacter != NULL && L == LinkCharacter->GetWeapon())
	{
		return true;
	}

	return false;
}

bool AUTWeap_LinkGun::IsLinkable(AActor* Other)
{
	APawn* Pawn = Cast<APawn>(Other);
	if (Pawn != nullptr) // && UTC.bProjTarget ~~~~ && UTC->GetActorEnableCollision()
	{
		// @! TODO : Allow/disallow Vehicles
		//if (Cast<AVehicle>(Pawn) != nullptr)
		//{
		//  return false;
		//}

		AUTCharacter* UTC = Cast<AUTCharacter>(Other);
		if (UTC != nullptr)
		{
			AUTWeap_LinkGun* OtherLinkGun = Cast<AUTWeap_LinkGun>(UTC->GetWeapon());
			if (OtherLinkGun != nullptr)
			{
				if (OtherLinkGun->LinkTarget == Cast<AActor>(GetUTOwner()))
				{
					return false;
				}

				// @! TODO: Loop and prevent circular chains

				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				return (GS != NULL && GS->OnSameTeam(UTC, UTOwner));
			}
		}
	}
	//@! TODO : Non-Pawns like Objectives

	return false;
}

void AUTWeap_LinkGun::SetLinkTo(AActor* Other)
{
	if (LinkTarget != nullptr)
	{
		RemoveLink(1 + Links, UTOwner);
	}

	LinkTarget = Other;

	if (LinkTarget != nullptr && Cast<AUTCharacter>(LinkTarget) != nullptr)
	{
		if (!AddLink(1 + Links, UTOwner))
		{
			bFeedbackDeath = true;
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), LinkEstablishedOtherSound, LinkTarget, SRT_None);
		}
	}
}

bool AUTWeap_LinkGun::AddLink(int32 Size, AUTCharacter* Starter)
{
	AUTWeap_LinkGun* OtherLinkGun;
	AUTCharacter* LinkCharacter = Cast<AUTCharacter>(LinkTarget);

	if (LinkCharacter != nullptr && !bFeedbackDeath)
	{
		if (LinkCharacter == Starter)
		{
			return false;
		}
		else
		{
			OtherLinkGun = Cast<AUTWeap_LinkGun>(LinkCharacter->GetWeapon());
			if (OtherLinkGun != nullptr)
			{
				if (OtherLinkGun->AddLink(Size, Starter))
				{
					OtherLinkGun->Links += Size;
				}
				else
				{
					return false;
				}
			}
		}
	}
	return true;
}

void AUTWeap_LinkGun::RemoveLink(int32 Size, AUTCharacter* Starter)
{
	AUTWeap_LinkGun* OtherLinkGun;
	AUTCharacter* LinkCharacter = Cast<AUTCharacter>(LinkTarget);

	if (LinkCharacter != NULL && !bFeedbackDeath)
	{
		if (LinkCharacter != Starter)
		{
			OtherLinkGun = Cast<AUTWeap_LinkGun>(LinkCharacter->GetWeapon());
			if (OtherLinkGun != NULL)
			{
				OtherLinkGun->RemoveLink(Size, Starter);
				OtherLinkGun->Links -= Size;
			}
		}
	}
}

void AUTWeap_LinkGun::ClearLinksTo()
{
	if (LinkTarget != NULL)
	{
		RemoveLink(1 + Links, UTOwner);
		LinkTarget = NULL;
	}
}

void AUTWeap_LinkGun::ClearLinksFrom()
{
	Links = 0;
}

void AUTWeap_LinkGun::ClearLinks()
{
	ClearLinksFrom();
	ClearLinksTo();
}

void AUTWeap_LinkGun::PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	FVector ModifiedTargetLoc = TargetLoc;

	if (LinkedBio != NULL) 
	{
		if (LinkedBio->IsPendingKillPending() || FireMode != 1 || UTOwner == NULL || ((UTOwner->GetActorLocation() - LinkedBio->GetActorLocation()).Size() > LinkedBio->MaxLinkDistance + InstantHitInfo[1].TraceRange))
		{
			LinkedBio = NULL;
		}
		else
		{
			// verify line of sight
			FHitResult Hit;
			static FName NAME_BioLinkTrace(TEXT("BioLinkTrace"));
			bool bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, SpawnLocation, LinkedBio->GetActorLocation(), COLLISION_TRACE_WEAPON, FCollisionQueryParams(NAME_BioLinkTrace, true, UTOwner));
			if ((bBlockingHit || (Hit.Actor != NULL)) && !Cast<AUTProj_BioShot>(Hit.Actor.Get()))
			{
				LinkedBio = NULL;
			}
		}
	}
	if (LinkedBio != NULL)
	{
		ModifiedTargetLoc = LinkedBio->GetActorLocation();
	}
	Super::PlayImpactEffects(ModifiedTargetLoc, FireMode, SpawnLocation, SpawnRotation);

	// color beam if linked
	if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL)
	{
		static FName NAME_TeamColor(TEXT("TeamColor"));
		bool bGotTeamColor = false;
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (Cast<IUTTeamInterface>(LinkTarget) != NULL && GS != NULL)
		{
			uint8 TeamNum = Cast<IUTTeamInterface>(LinkTarget)->GetTeamNum();
			if (GS->Teams.IsValidIndex(TeamNum) && GS->Teams[TeamNum] != NULL)
			{
				MuzzleFlash[CurrentFireMode]->SetVectorParameter(NAME_TeamColor, FVector(GS->Teams[TeamNum]->TeamColor.R, GS->Teams[TeamNum]->TeamColor.G, GS->Teams[TeamNum]->TeamColor.B));
				bGotTeamColor = true;
			}
		}
		if (!bGotTeamColor)
		{
			MuzzleFlash[CurrentFireMode]->ClearParameter(NAME_TeamColor);
		}
	}
}

void AUTWeap_LinkGun::StopFire(uint8 FireModeNum)
{
	LinkedBio = NULL;
	Super::StopFire(FireModeNum);
}

// reset links
bool AUTWeap_LinkGun::PutDown()
{
	if (Super::PutDown())
	{
		ClearLinks();
		return true;
	}
	else
	{
		return false;
	}
}

void AUTWeap_LinkGun::ServerStopFire_Implementation(uint8 FireModeNum)
{
	LinkedBio = NULL;
	Super::ServerStopFire_Implementation(FireModeNum);
}

void AUTWeap_LinkGun::OnMultiPress_Implementation(uint8 OtherFireMode)
{
	if (CurrentFireMode == 1 && OtherFireMode == 0 && !IsLinkPulsing())
	{
		bPendingBeamPulse = true;
	}
	Super::OnMultiPress_Implementation(OtherFireMode);
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
	if (UTOwner != NULL && LinkTarget == NULL && CurrentFireMode == 1 && InstantHitInfo.IsValidIndex(1) && !bPendingBeamPulse)
	{
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if ( B != NULL && B->WeaponProficiencyCheck() && B->GetEnemy() != NULL && B->GetTarget() == B->GetEnemy() &&
			(B->IsCharging() || B->GetSquad()->MustKeepEnemy(B->GetEnemy()) || B->RelativeStrength(B->GetEnemy()) < 0.0f) )
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

void AUTWeap_LinkGun::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeap_LinkGun, Links, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTWeap_LinkGun, LinkTarget, COND_OwnerOnly);
}

void AUTWeap_LinkGun::DebugSetLinkGunLinks(int32 newLinks)
{
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Black, FString::Printf(TEXT("DebugSetLinkGunLinks, oldLinks: %i, newLinks: %i"), Links, newLinks));
	Links = newLinks;
}

void AUTWeap_LinkGun::FiringExtraUpdated_Implementation(uint8 NewFlashExtra, uint8 InFireMode)
{
	if (NewFlashExtra > 0 && InFireMode == 1)
	{
		LastBeamPulseTime = GetWorld()->TimeSeconds;
		// use an extra muzzle flash slot at the end for the pulse effect
		if (MuzzleFlash.IsValidIndex(FiringState.Num()) && MuzzleFlash[FiringState.Num()] != NULL)
		{
			MuzzleFlash[FiringState.Num()]->ActivateSystem();
		}
		PlayWeaponAnim(PulseAnim, PulseAnimHands);
	}
}
