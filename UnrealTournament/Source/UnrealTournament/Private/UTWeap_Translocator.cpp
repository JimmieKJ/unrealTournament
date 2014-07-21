

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
	ConsumeAmmo(CurrentFireMode);

	if (!FireShotOverride() && GetUTOwner() != NULL) // script event may kill user
	{
		if (CurrentFireMode == 0)
		{
			//No disk. Shoot one
			if (TransDisk == NULL)
			{
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
			}
		}
		else
		{
			if (TransDisk != NULL)
			{
				if (TransDisk->TransState == TLS_Disrupted)
				{
					FUTPointDamageEvent Event;
					float AdjustedMomentum = 1000.0f;
					Event.Damage = TelefragDamage;
					Event.DamageTypeClass = TransFailDamageType;
					Event.HitInfo = FHitResult(UTOwner, UTOwner->CapsuleComponent, UTOwner->GetActorLocation(), FVector(0.0f,0.0f,1.0f));
					Event.ShotDirection = GetVelocity().SafeNormal();
					Event.Momentum = Event.ShotDirection * AdjustedMomentum;

					UTOwner->TakeDamage(TelefragDamage, Event, UTOwner->GetController(), UTOwner);
				}
				else
				{
					FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(UTOwner->CapsuleComponent->GetUnscaledCapsuleRadius(), UTOwner->CapsuleComponent->GetUnscaledCapsuleHalfHeight());
					FVector WarpLocation = TransDisk->GetActorLocation() + FVector(0.0f, 0.0f, PlayerCapsule.GetCapsuleHalfHeight());
					FRotator WarpRotation(0.0f, UTOwner->GetActorRotation().Yaw, 0.0f);
					
					//TODO: check if we can actually warp
					UTOwner->DropFlag();

					UTOwner->IncrementFlashCount(CurrentFireMode);

					if (UTOwner->TeleportTo(WarpLocation, WarpRotation))
					{
						TArray<FOverlapResult> Overlaps;
						if (GetWorld()->OverlapMulti(Overlaps, WarpLocation, WarpRotation.Quaternion(),
							PlayerCapsule, FCollisionQueryParams(FName(TEXT("TransOverlap")), false, UTOwner),
							FCollisionObjectQueryParams(ECollisionChannel::ECC_Pawn)))
						{

							for (int i = 0; i < Overlaps.Num(); i++)
							{
								FUTPointDamageEvent Event;
								float AdjustedMomentum = 1000.0f;
								Event.Damage = TelefragDamage;
								Event.DamageTypeClass = TelefragDamageType;
								Event.HitInfo = FHitResult(UTOwner, UTOwner->CapsuleComponent, WarpLocation, WarpRotation.Vector());
								Event.ShotDirection = GetVelocity().SafeNormal();
								Event.Momentum = Event.ShotDirection * AdjustedMomentum;

								//TODO: dont hit team mates
								AUTCharacter* UTP = Cast<AUTCharacter>(Overlaps[i].GetActor());
								if (UTP != NULL && (UTP->GetTeamNum() == 255 || UTP->GetTeamNum() != UTOwner->GetTeamNum()))
								{
									Overlaps[i].GetActor()->TakeDamage(TelefragDamage, Event, UTOwner->GetController(), UTOwner);
								}
							}
						}
					}
					UUTGameplayStatics::UTPlaySound(GetWorld(), TeleSound, UTOwner, SRT_AllButOwner);
				}
				ClearDisk();
			}
		}

		PlayFiringEffects();
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