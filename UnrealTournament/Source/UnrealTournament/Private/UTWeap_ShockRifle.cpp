// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_ShockRifle.h"
#include "UTProj_ShockBall.h"
#include "UTCanvasRenderTarget2D.h"
#include "StatNames.h"
#include "StatNames.h"

AUTWeap_ShockRifle::AUTWeap_ShockRifle(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ClassicGroup = 4;
	BaseAISelectRating = 0.65f;
	BasePickupDesireability = 0.65f;
	ScreenMaterialID = 5;
	LastClientKillTime = -100000.0f;
	bFPIgnoreInstantHitFireOffset = false; // needed so any fire offset set for shock ball is matched by primary for standing combos
	FOVOffset = FVector(0.6f, 0.9f, 1.2f);

	KillStatsName = NAME_ShockBeamKills;
	AltKillStatsName = NAME_ShockCoreKills;
	DeathStatsName = NAME_ShockBeamDeaths;
	AltDeathStatsName = NAME_ShockCoreDeaths;
	HitsStatsName = NAME_ShockBeamHits;
	ShotsStatsName = NAME_ShockBeamShots;
}

void AUTWeap_ShockRifle::AttachToOwner_Implementation()
{
	Super::AttachToOwner_Implementation();

	if (GetNetMode() != NM_DedicatedServer && Mesh != NULL && ScreenMaterialID < Mesh->GetNumMaterials())
	{
		ScreenMI = Mesh->CreateAndSetMaterialInstanceDynamic(ScreenMaterialID);
		ScreenTexture = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), 64, 64);
		ScreenTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		ScreenTexture->OnCanvasRenderTargetUpdate.AddDynamic(this, &AUTWeap_ShockRifle::UpdateScreenTexture);
		ScreenMI->SetTextureParameterValue(FName(TEXT("ScreenTexture")), ScreenTexture);
	}
}

void AUTWeap_ShockRifle::UpdateScreenTexture(UCanvas* C, int32 Width, int32 Height)
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

void AUTWeap_ShockRifle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ScreenTexture != NULL && Mesh->IsRegistered() && GetWorld()->TimeSeconds - Mesh->LastRenderTime < 1.0f)
	{
		ScreenTexture->UpdateResource();
	}
}

UAnimMontage* AUTWeap_ShockRifle::GetFiringAnim(uint8 FireMode, bool bOnHands) const
{
	if (FireMode == 0 && bPlayComboEffects && ComboFireAnim != NULL)
	{
		return (bOnHands ? ComboFireAnimHands : ComboFireAnim);
	}
	else
	{
		return Super::GetFiringAnim(FireMode, bOnHands);
	}
}

void AUTWeap_ShockRifle::PlayFiringEffects()
{
	Super::PlayFiringEffects();

	if (bPlayComboEffects && ShouldPlay1PVisuals())
	{
		Play1PComboEffects();
		bPlayComboEffects = false;
	}
}

void AUTWeap_ShockRifle::HitScanTrace(const FVector& StartLocation, const FVector& EndTrace, FHitResult& Hit, float PredictionTime)
{
	Super::HitScanTrace(StartLocation, EndTrace, Hit, PredictionTime);

	bPlayComboEffects = (Cast<AUTProj_ShockBall>(Hit.GetActor()) != NULL);
}

bool AUTWeap_ShockRifle::WaitingForCombo()
{
	if (ComboTarget != NULL && !ComboTarget->bPendingKillPending && !ComboTarget->bExploded)
	{
		return true;
	}
	else
	{
		ComboTarget = NULL;
		return false;
	}
}

void AUTWeap_ShockRifle::DoCombo()
{
	ComboTarget = NULL;
	if (UTOwner != NULL)
	{
		UTOwner->StartFire(0);
	}
}

bool AUTWeap_ShockRifle::IsPreparingAttack_Implementation()
{
	// if bot is allowed to do a moving combo then prioritize evasive action prior to combo time
	return !bMovingComboCheckResult && WaitingForCombo();
}

float AUTWeap_ShockRifle::SuggestAttackStyle_Implementation()
{
	return -0.4f;
}

float AUTWeap_ShockRifle::GetAISelectRating_Implementation()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL || B->GetEnemy() == NULL || Cast<APawn>(B->GetTarget()) == NULL)
	{
		return BaseAISelectRating;
	}
	else if (WaitingForCombo())
	{
		return 1.5f;
	}
	else if (!B->WeaponProficiencyCheck())
	{
		return BaseAISelectRating;
	}
	else
	{
		FVector EnemyLoc = B->GetEnemyLocation(B->GetEnemy(), true);
		if (B->IsStopped())
		{
			if (!B->LineOfSightTo(B->GetEnemy()) && (EnemyLoc - UTOwner->GetActorLocation()).Size() < 11000.0f)
			{
				return BaseAISelectRating + 0.5f;
			}
			else
			{
				return BaseAISelectRating + 0.3f;
			}
		}
		else if ((EnemyLoc - UTOwner->GetActorLocation()).Size() > 3500.0f)
		{
			return BaseAISelectRating + 0.1f;
		}
		else if (EnemyLoc.Z > UTOwner->GetActorLocation().Z + 325.0f)
		{
			return BaseAISelectRating + 0.15f;
		}
		else
		{
			return BaseAISelectRating;
		}
	}
}

bool AUTWeap_ShockRifle::ShouldAIDelayFiring_Implementation()
{
	if (!WaitingForCombo())
	{
		return false;
	}
	else if (bMovingComboCheckResult)
	{
		return true;
	}
	else
	{
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if (B != NULL && !B->IsStopped())
		{
			// bot's too low skill to do combo since it started moving
			ComboTarget->ClearBotCombo();
			ComboTarget = NULL;
			return false;
		}
		else
		{
			return true;
		}
	}
}

bool AUTWeap_ShockRifle::CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc)
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL)
	{
		return Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, BestFireMode, OptimalTargetLoc);
	}
	else if (WaitingForCombo() && (Target == ComboTarget || Target == B->GetTarget()))
	{
		BestFireMode = 0;
		return true;
	}
	else if (Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, BestFireMode, OptimalTargetLoc))
	{
		if (bPreferCurrentMode)
		{
			return true;
		}
		else
		{
			if (Cast<APawn>(Target) == NULL)
			{
				BestFireMode = 0;
			}
			/*else if (!B.LineOfSightTo(B.Enemy))
			{
				if ((ComboTarget != None) && !ComboTarget.bDeleteMe && B.CanCombo())
				{
					bWaitForCombo = true;
					return 0;
				}
				ComboTarget = None;
				if (B.CanCombo() && B.ProficientWithWeapon())
				{
					bRegisterTarget = true;
					return 1;
				}
				return 0;
			}*/
			else
			{
				float EnemyDist = (TargetLoc - UTOwner->GetActorLocation()).Size();
				const AUTProjectile* DefAltProj = (ProjClass.IsValidIndex(1) && ProjClass[1] != NULL) ? ProjClass[1].GetDefaultObject() : NULL;
				const float AltSpeed = (DefAltProj != NULL && DefAltProj->ProjectileMovement != NULL) ? DefAltProj->ProjectileMovement->InitialSpeed : FLT_MAX;

				if (EnemyDist > 4.0f * AltSpeed)
				{
					bPlanningCombo = false;
					BestFireMode = 0;
				}
				else
				{
					ComboTarget = NULL;
					if (EnemyDist > 5500.0f && FMath::FRand() < 0.5f)
					{
						BestFireMode = 0;
					}
					else if (B->CanCombo() && B->WeaponProficiencyCheck())
					{
						bPlanningCombo = true;
						BestFireMode = 1;
					}
					// projectile is better in close due to its size
					else
					{
						AUTCharacter* EnemyChar = Cast<AUTCharacter>(Target);
						if (EnemyDist < 2200.0f && EnemyChar != NULL && EnemyChar->GetWeapon() != NULL && EnemyChar->GetWeapon()->GetClass() != GetClass() && B->WeaponProficiencyCheck())
						{
							BestFireMode = (FMath::FRand() < 0.3f) ? 0 : 1;
						}
						else
						{
							BestFireMode = (FMath::FRand() < 0.7f) ? 0 : 1;
						}
					}
				}
			}
			return true;
		}
	}
	else if (bDirectOnly)
	{
		return false;
	}
	else if ((bPreferCurrentMode && bPlanningCombo) || GetWorld()->TimeSeconds - LastPredictiveComboCheckTime >= 1.0f)
	{
		// see if shock combo could hit enemy if it came around the corner
		LastPredictiveComboCheckTime = GetWorld()->TimeSeconds;
		if (!bPlanningCombo && !B->CanCombo()) // check that we have the skills if we haven't already
		{
			bPlanningCombo = false;
			return false;
		}
		else if (bPreferCurrentMode && !PredictiveComboTargetLoc.IsZero() && !GetWorld()->LineTraceTestByChannel(GetFireStartLoc(1), PredictiveComboTargetLoc, COLLISION_TRACE_WEAPONNOCHARACTER, FCollisionQueryParams(FName(TEXT("PredictiveCombo")), true, UTOwner)))
		{
			OptimalTargetLoc = PredictiveComboTargetLoc;
			BestFireMode = 1;
			bPlanningCombo = true;
			return true;
		}
		else
		{
			PredictiveComboTargetLoc = FVector::ZeroVector;
			TArray<FVector> FoundPoints;
			B->GuessAppearancePoints(Target, TargetLoc, true, FoundPoints);
			if (FoundPoints.Num() > 0)
			{
				int32 StartIndex = FMath::RandHelper(FoundPoints.Num());
				int32 i = StartIndex;
				do 
				{
					i = (i + 1) % FoundPoints.Num();
					if (!GetWorld()->LineTraceTestByChannel(GetFireStartLoc(1), FoundPoints[i], COLLISION_TRACE_WEAPONNOCHARACTER, FCollisionQueryParams(FName(TEXT("PredictiveCombo")), true, UTOwner)))
					{
						PredictiveComboTargetLoc = FoundPoints[i];
						break;
					}
				} while (i != StartIndex);

				bPlanningCombo = !PredictiveComboTargetLoc.IsZero();
				return bPlanningCombo;
			}
			else
			{
				bPlanningCombo = false;
				return false;
			}
		}
	}
	else
	{
		return false;
	}
}

AUTProjectile* AUTWeap_ShockRifle::FireProjectile()
{
	AUTProjectile* Result = Super::FireProjectile();
	if (bPlanningCombo && UTOwner != NULL)
	{
		AUTProj_ShockBall* ShockBall = Cast<AUTProj_ShockBall>(Result);
		if (ShockBall != NULL)
		{
			ShockBall->StartBotComboMonitoring();
			ComboTarget = ShockBall;
			AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
			if (B != NULL)
			{
				bMovingComboCheckResult = B->MovingComboCheck();
			}
			bPlanningCombo = false;
			PredictiveComboTargetLoc = FVector::ZeroVector;
		}
	}
	return Result;
}

int32 AUTWeap_ShockRifle::GetWeaponKillStats(AUTPlayerState * PS) const
{
	int32 KillCount = Super::GetWeaponKillStats(PS);
	if (PS)
	{
		KillCount += PS->GetStatsValue(NAME_ShockComboKills);
	}
	return KillCount;
}

int32 AUTWeap_ShockRifle::GetWeaponDeathStats(AUTPlayerState * PS) const
{
	int32 DeathCount = Super::GetWeaponDeathStats(PS);
	if (PS)
	{
		DeathCount += PS->GetStatsValue(NAME_ShockComboDeaths);
	}
	return DeathCount;
}

void AUTWeap_ShockRifle::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	//Store this now since it might get cleared below. bPlayComboEffects is set by the shock core when combo'd
	bool bIsCombo = bPlayComboEffects;
	Super::FireInstantHit(bDealDamage, OutHit);

	//FlashExtra 1 will play the ComboEffects for the other clients 
	if (Role == ROLE_Authority && UTOwner != nullptr && bIsCombo)
	{
		UTOwner->SetFlashExtra(1, CurrentFireMode);
	}
}

void AUTWeap_ShockRifle::FiringExtraUpdated_Implementation(uint8 NewFlashExtra, uint8 InFireMode)
{
	bPlayComboEffects = (InFireMode == 0 && (NewFlashExtra > 0));
}