#include "UnrealTournament.h"
#include "UTWeaponLocker.h"
#include "UTWorldSettings.h"
#include "UnrealNetwork.h"
#include "UTPickupMessage.h"
#include "UTGameVolume.h"

AUTWeaponLocker::AUTWeaponLocker(const FObjectInitializer& OI)
	: Super(OI)
{
	Collision->InitCapsuleSize(100.0f, 120.0f);
	PickupMessageString = NSLOCTEXT("UnrealTournament", "WeaponLocker", "");

	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(60.0f, 0.0f, 0.0f)));
	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(-60.0f, 0.0f, 0.0f)));
	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(0.0f, 60.0f, 0.0f)));
	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(0.0f, -60.0f, 0.0f)));
	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(60.0f, 60.0f, 0.0f)));
	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(60.0f, -60.0f, 0.0f)));
	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(-60.0f, 60.0f, 0.0f)));
	WeaponPlacements.Add(FTransform(FQuat(FRotator(0.0f, 0.0f, 90.0f)), FVector(-60.0f, -60.0f, 0.0f)));
}

void AUTWeaponLocker::BeginPlay()
{
	if (!IsPendingKillPending())
	{
		AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
		if (WS != NULL)
		{
			WS->PerPlayerPickups.Add(this);
		}
	}

	Super::BeginPlay();

	if (TimerEffect != NULL)
	{
		TimerEffect->SetVisibility(true); // note: HiddenInGame used to hide when weapon is available, weapon stay, etc
	}
	if (Role == ROLE_Authority)
	{
		SetWeaponList(WeaponList);
	}
	else
	{
		WeaponListUpdated();
	}

	// associate as team locker with team volume I am in
	TArray<UPrimitiveComponent*> OverlappingComponents;
	Collision->GetOverlappingComponents(OverlappingComponents);
	MyGameVolume = nullptr;
	int32 BestPriority = -1.f;

	for (auto CompIt = OverlappingComponents.CreateIterator(); CompIt; ++CompIt)
	{
		UPrimitiveComponent* OtherComponent = *CompIt;
		if (OtherComponent && OtherComponent->bGenerateOverlapEvents)
		{
			AUTGameVolume* V = Cast<AUTGameVolume>(OtherComponent->GetOwner());
			if (V && V->Priority > BestPriority)
			{
				if (V->IsOverlapInVolume(*Collision))
				{
					BestPriority = V->Priority;
					MyGameVolume = V;
				}
			}
		}
	}
	if (MyGameVolume && MyGameVolume->bIsTeamSafeVolume)
	{
		MyGameVolume->TeamLockers.AddUnique(this);
	}
	else
	{
		MyGameVolume = nullptr;
	}
}

void AUTWeaponLocker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	if (WS != NULL)
	{
		WS->PerPlayerPickups.Remove(this);
	}
}

void AUTWeaponLocker::SetWeaponList(TArray<FWeaponLockerItem> InList)
{
	// copy the array excluding NULLs and duplicates
	WeaponList.Reset();
	for (const FWeaponLockerItem& NewItem : InList)
	{
		if (NewItem.WeaponType != NULL)
		{
			bool bFound = false;
			for (FWeaponLockerItem& OtherItem : WeaponList)
			{
				if (OtherItem.WeaponType == NewItem.WeaponType)
				{
					OtherItem.ExtraAmmo += NewItem.ExtraAmmo;
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				WeaponList.Add(NewItem);
			}
		}
	}
	// clamp max possible weapon types so we can't break replication with bad arrays
	WeaponList.SetNum(FMath::Min<int32>(WeaponList.Num(), 15), true);

	if (GetNetMode() != NM_DedicatedServer)
	{
		WeaponListUpdated();
	}
}

void AUTWeaponLocker::WeaponListUpdated()
{
	for (UMeshComponent* NextMesh : WeaponMeshes)
	{
		if (NextMesh != NULL)
		{
			TArray<USceneComponent*> ChildComps;
			NextMesh->GetChildrenComponents(true, ChildComps);
			for (USceneComponent* Child : ChildComps)
			{
				Child->DestroyComponent(false);
			}
			NextMesh->DestroyComponent(false);
		}
	}
	WeaponMeshes.Reset();

	for (int32 i = 0; i < FMath::Min<int32>(WeaponList.Num(), WeaponPlacements.Num()); i++)
	{
		UMeshComponent* NewMesh = nullptr;
		AUTPickupInventory::CreatePickupMesh(this, NewMesh, WeaponList[i].WeaponType, 0.0f, FRotator::ZeroRotator, false);
		if (NewMesh != NULL)
		{
			NewMesh->SetRelativeTransform(NewMesh->GetRelativeTransform() * WeaponPlacements[i]);
			WeaponMeshes.Add(NewMesh);
		}
	}
}

bool AUTWeaponLocker::IsTaken(APawn* TestPawn)
{
	for (int32 i = Customers.Num() - 1; i >= 0; i--)
	{
		if (Customers[i].P == NULL || Customers[i].P->bTearOff || Customers[i].P->IsPendingKillPending())
		{
			Customers.RemoveAt(i);
		}
		else if (Customers[i].P == TestPawn)
		{
			return (GetWorld()->TimeSeconds < Customers[i].NextPickupTime);
		}
	}
	return false;
}

float AUTWeaponLocker::GetRespawnTimeOffset(APawn* Asker) const
{
	if (!State.bActive)
	{
		return Super::GetRespawnTimeOffset(Asker);
	}
	else
	{
		for (int32 i = Customers.Num() - 1; i >= 0; i--)
		{
			if (Customers[i].P == Asker)
			{
				return (Customers[i].NextPickupTime - GetWorld()->TimeSeconds);
			}
		}
		return -100000.0f;
	}
}

void AUTWeaponLocker::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (State.bActive)
	{
		// note that we don't currently call AllowPickupBy() and associated GameMode/Mutator overrides in the weapon stay case
		// in part due to client synchronization issues
		if (!IsTaken(TouchedBy) && Cast<AUTCharacter>(TouchedBy) != NULL && !((AUTCharacter*)TouchedBy)->IsRagdoll() && ((AUTCharacter*)TouchedBy)->bCanPickupItems)
		{
			if (Role == ROLE_Authority)
			{
				GiveTo(TouchedBy);
			}
			DisabledByTouchBy(TouchedBy);
			PlayTakenEffects(false);
			UUTGameplayStatics::UTPlaySound(GetWorld(), TakenSound, TouchedBy, SRT_IfSourceNotReplicated, false, FVector::ZeroVector, NULL, NULL, false);
			if (MyGameVolume)
			{
				// also notify/disable linked pickups
				for (int32 i = 0; i < MyGameVolume->TeamLockers.Num(); i++)
				{
					AUTWeaponLocker* Locker = MyGameVolume->TeamLockers[i];
					if (Locker && !Locker->IsTaken(TouchedBy))
					{
						Locker->DisabledByTouchBy(TouchedBy);
					}
				}
			}
		}
	}
}

void AUTWeaponLocker::DisabledByTouchBy(APawn* TouchedBy)
{
	new(Customers) FWeaponPickupCustomer(TouchedBy, GetWorld()->TimeSeconds + RespawnTime);
	if (!GetWorldTimerManager().IsTimerActive(CheckTouchingHandle))
	{
		GetWorldTimerManager().SetTimer(CheckTouchingHandle, this, &AUTWeaponLocker::CheckTouching, RespawnTime, false);
	}
	if (TouchedBy->IsLocallyControlled())
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(TouchedBy->Controller);
		if (PC != NULL)
		{
			// TODO: does not properly support splitscreen
			if (BaseEffect != NULL && BaseTemplateTaken != NULL)
			{
				BaseEffect->SetTemplate(BaseTemplateTaken);
			}
			if (TimerEffect != NULL)
			{
				TimerEffect->SetFloatParameter(NAME_Progress, 0.0f);
				TimerEffect->SetFloatParameter(NAME_RespawnTime, RespawnTime);
				TimerEffect->SetHiddenInGame(false);
			}
			PC->AddPerPlayerPickup(this);
		}
	}
}

void AUTWeaponLocker::CheckTouching()
{
	TArray<AActor*> Touching;
	GetOverlappingActors(Touching, APawn::StaticClass());
	for (AActor* TouchingActor : Touching)
	{
		APawn* P = Cast<APawn>(TouchingActor);
		if (P != NULL)
		{
			ProcessTouch(P);
		}
	}
	// see if we should reset the timer
	float NextCheckInterval = 0.0f;
	for (const FWeaponPickupCustomer& PrevCustomer : Customers)
	{
		NextCheckInterval = FMath::Max<float>(NextCheckInterval, PrevCustomer.NextPickupTime - GetWorld()->TimeSeconds);
	}
	if (NextCheckInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(CheckTouchingHandle, this, &AUTWeaponLocker::CheckTouching, NextCheckInterval, false);
	}
}

void AUTWeaponLocker::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL)
	{
		P->DeactivateSpawnProtection();
		if (Cast<APlayerController>(P->GetController()) != nullptr)
		{
			((APlayerController*)P->GetController())->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, P->PlayerState, NULL, GetClass());
		}
		for (const FWeaponLockerItem& Item : WeaponList)
		{
			AUTInventory* Existing = P->FindInventoryType(Item.WeaponType, true);
			if (Existing == NULL || !Existing->StackPickup(nullptr))
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Params.Instigator = P;
				P->AddInventory(GetWorld()->SpawnActor<AUTInventory>(Item.WeaponType, GetActorLocation(), GetActorRotation(), Params), true);
			}
			if (Item.ExtraAmmo != 0)
			{
				//P->AddAmmo(FStoredAmmo{Item.WeaponType, Item.ExtraAmmo});
			}

			//Add to the stats pickup count
			const AUTInventory* Inventory = Item.WeaponType.GetDefaultObject();
			if (Inventory && Inventory->StatsNameCount != NAME_None)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(P->PlayerState);
				if (PS)
				{
					PS->ModifyStatsValue(Inventory->StatsNameCount, 1);
					if (PS->Team)
					{
						PS->Team->ModifyStatsValue(Inventory->StatsNameCount, 1);
					}

					AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
					if (GS != nullptr)
					{
						GS->ModifyStatsValue(Inventory->StatsNameCount, 1);
					}

					//Send the pickup message to the spectators
					AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
					if (UTGameMode != nullptr)
					{
						UTGameMode->BroadcastSpectatorPickup(PS, Inventory->StatsNameCount, Inventory->GetClass());
					}
				}
			}
		}
	}
}

void AUTWeaponLocker::PlayTakenEffects(bool bReplicate)
{
	if (bReplicate)
	{
		Super::PlayTakenEffects(bReplicate);
	}
	else if (GetNetMode() != NM_DedicatedServer)
	{
		AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
		if (WS == NULL || WS->EffectIsRelevant(this, GetActorLocation(), true, false, 10000.0f, 1000.0f, false))
		{
			UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAttached(TakenParticles, RootComponent, NAME_None, TakenEffectTransform.GetLocation(), TakenEffectTransform.GetRotation().Rotator());
			if (PSC != NULL)
			{
				PSC->SetRelativeScale3D(TakenEffectTransform.GetScale3D());
			}
		}
		for (const FWeaponLockerItem& Item : WeaponList)
		{
			TakenSound = (Item.WeaponType != NULL) ? TakenSound = Item.WeaponType->GetDefaultObject<AUTWeapon>()->PickupSound : GetClass()->GetDefaultObject<AUTPickupInventory>()->TakenSound;
			UUTGameplayStatics::UTPlaySound(GetWorld(), TakenSound, this, SRT_None, false, FVector::ZeroVector, NULL, NULL, false);
		}
	}
}

void AUTWeaponLocker::SetPickupHidden(bool bNowHidden)
{
	Super::SetPickupHidden(bNowHidden);
	for (UMeshComponent* ItemMesh : WeaponMeshes)
	{
		if (ItemMesh != nullptr)
		{
			ItemMesh->SetHiddenInGame(bNowHidden);
		}
	}
}

void AUTWeaponLocker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTWeaponLocker, WeaponList);
}

float AUTWeaponLocker::BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float TotalDistance)
{
	return IsTaken(Asker) ? 0.0f : 1.0f; // FIXME: real rating
}
float AUTWeaponLocker::DetourWeight_Implementation(APawn* Asker, float TotalDistance)
{
	AUTBot* B = Cast<AUTBot>(Asker->Controller);
	return (B != nullptr && B->NeedsWeapon() && !IsTaken(Asker)) ? 1.0f : 0.0f;
}