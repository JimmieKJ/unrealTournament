

#include "UnrealTournament.h"
#include "UTWeap_Translocator.h"
#include "UTProj_TransDisk.h"
#include "UTWeaponStateFiringOnce.h"
#include "UnrealNetwork.h"


AUTWeap_Translocator::AUTWeap_Translocator(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateFiringOnce>(TEXT("FiringState0")).SetDefaultSubobjectClass<UUTWeaponStateFiringOnce>(TEXT("FiringState1")))
{
	if (FiringState.Num() > 1)
	{
#if WITH_EDITORONLY_DATA
		FiringStateType[0] = UUTWeaponStateFiringOnce::StaticClass();
		FiringStateType[1] = UUTWeaponStateFiringOnce::StaticClass();
#endif
	}
	TelefragDamage = 1337.0f;

	BringUpTime = 0.35f;
	PutDownTime = 0.25f;

	AmmoCost.Add(0);
	AmmoCost.Add(1);
	Ammo = 8;
	MaxAmmo = 8;
	AmmoRechargeRate = 1.0f;

	RecallFireInterval = 0.2f;
}

void AUTWeap_Translocator::ConsumeAmmo(uint8 FireModeNum)
{
	Super::ConsumeAmmo(FireModeNum);

	if ((FireModeNum == 1 || Ammo < MaxAmmo) && !GetWorldTimerManager().IsTimerActive(this, &AUTWeap_Translocator::RechargeTimer) && !bPendingKillPending)
	{
		GetWorldTimerManager().SetTimer(this, &AUTWeap_Translocator::RechargeTimer, AmmoRechargeRate, true);
	}
}

void AUTWeap_Translocator::RechargeTimer()
{
	AddAmmo(1);
	if (Ammo >= MaxAmmo)
	{
		GetWorldTimerManager().ClearTimer(this, &AUTWeap_Translocator::RechargeTimer);
	}
}
void AUTWeap_Translocator::OnRep_Ammo()
{
	Super::OnRep_Ammo();
	if (Ammo >= MaxAmmo)
	{
		GetWorldTimerManager().ClearTimer(this, &AUTWeap_Translocator::RechargeTimer);
	}
}

void AUTWeap_Translocator::OnRep_TransDisk()
{

}

void AUTWeap_Translocator::ClearDisk()
{
	if (TransDisk != NULL)
	{
		TransDisk->Explode(TransDisk->GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
	TransDisk = NULL;
}

void AUTWeap_Translocator::FireShot()
{
	UTOwner->DeactivateSpawnProtection();

	if (!FireShotOverride() && GetUTOwner() != NULL) // script event may kill user
	{
		if (CurrentFireMode == 0)
		{
			//No disk. Shoot one
			if (TransDisk == NULL)
			{
				ConsumeAmmo(CurrentFireMode);
				if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
				{
					TransDisk = Cast<AUTProj_TransDisk>(FireProjectile());

					if (TransDisk != NULL)
					{
						TransDisk->MyTranslocator = this;
					}
				}

				UUTGameplayStatics::UTPlaySound(GetWorld(), ThrowSound, UTOwner, SRT_AllButOwner);
			}
			else
			{
				//remove disk
				ClearDisk();

				UUTGameplayStatics::UTPlaySound(GetWorld(), RecallSound, UTOwner, SRT_AllButOwner);

				// special recovery time for recall
				typedef void(UUTWeaponState::*WeaponTimerFunc)(void);
				if (Cast<UUTWeaponStateFiringOnce>(CurrentState) != NULL && GetWorldTimerManager().IsTimerActive(CurrentState, (WeaponTimerFunc)&UUTWeaponStateFiring::RefireCheckTimer))
				{
					GetWorldTimerManager().SetTimer(CurrentState, (WeaponTimerFunc)&UUTWeaponStateFiring::RefireCheckTimer, RecallFireInterval, false);
				}
			}
		}
		else if (TransDisk != NULL)
		{
			if (TransDisk->TransState == TLS_Disrupted)
			{
				ConsumeAmmo(CurrentFireMode); // well, we're probably about to die, but just in case

				FUTPointDamageEvent Event;
				float AdjustedMomentum = 1000.0f;
				Event.Damage = TelefragDamage;
				Event.DamageTypeClass = TransFailDamageType;
				Event.HitInfo = FHitResult(UTOwner, UTOwner->CapsuleComponent, UTOwner->GetActorLocation(), FVector(0.0f,0.0f,1.0f));
				Event.ShotDirection = GetVelocity().SafeNormal();
				Event.Momentum = Event.ShotDirection * AdjustedMomentum;

				UTOwner->TakeDamage(TelefragDamage, Event, TransDisk->DisruptedController, UTOwner);
			}
			else
			{
				UTOwner->IncrementFlashCount(CurrentFireMode);

				if (Role == ROLE_Authority)
				{
					FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(UTOwner->CapsuleComponent->GetUnscaledCapsuleRadius(), UTOwner->CapsuleComponent->GetUnscaledCapsuleHalfHeight());
					FVector WarpLocation = TransDisk->GetActorLocation();
					{
						FHitResult Hit;
						FVector EndTrace = WarpLocation + FVector(0.0f, 0.0f, PlayerCapsule.GetCapsuleHalfHeight());
						if (GetWorld()->SweepSingle(Hit, WarpLocation, EndTrace, FQuat::Identity, UTOwner->CapsuleComponent->GetCollisionObjectType(), FCollisionShape::MakeCapsule(PlayerCapsule.GetCapsuleRadius(), 1.0f), FCollisionQueryParams(FName(TEXT("Translocation")), false, UTOwner), UTOwner->CapsuleComponent->GetCollisionResponseToChannels()))
						{
							WarpLocation = Hit.Location;
							// note that offsetting downward here is not guaranteed to be any better; let FindTeleportSpot() deal with it
						}
						else
						{
							WarpLocation = EndTrace;
						}
					}
					FRotator WarpRotation(0.0f, UTOwner->GetActorRotation().Yaw, 0.0f);

					ECollisionChannel SavedObjectType = UTOwner->CapsuleComponent->GetCollisionObjectType();
					UTOwner->CapsuleComponent->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);
	
					// test first so we don't drop the flag on an unsuccessful teleport
					if (GetWorld()->FindTeleportSpot(UTOwner, WarpLocation, WarpRotation))
					{
						UTOwner->CapsuleComponent->SetCollisionObjectType(SavedObjectType);
						UTOwner->DropFlag();

						if (UTOwner->TeleportTo(WarpLocation, WarpRotation))
						{
							ConsumeAmmo(CurrentFireMode);
						}
					}
					else
					{
						UTOwner->CapsuleComponent->SetCollisionObjectType(SavedObjectType);
					}
				}
				UUTGameplayStatics::UTPlaySound(GetWorld(), TeleSound, UTOwner, SRT_AllButOwner);
			}
			ClearDisk();
		}

		PlayFiringEffects();
	}
	else
	{
		ConsumeAmmo(CurrentFireMode);
	}
	if (GetUTOwner() != NULL)
	{
		static FName NAME_FiredWeapon(TEXT("FiredWeapon"));
		GetUTOwner()->InventoryEvent(NAME_FiredWeapon);
	}
}

//Dont drop Weapon when killed
void AUTWeap_Translocator::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	if (TransDisk != NULL)
	{
		TransDisk->ShutDown();
	}
	Destroy();
}

void AUTWeap_Translocator::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeap_Translocator, TransDisk, COND_None);
}