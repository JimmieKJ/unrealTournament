// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProj_BioLauncherShot.h"
#include "UnrealNetwork.h"

AUTProj_BioLauncherShot::AUTProj_BioLauncherShot(const FObjectInitializer& OI)
	: Super(OI)
{
	WebHealth = 10;
	MaxWebReach = 1000.0f;
	MinWebStringDistance = 100.0f;
	RestTime = 10.0f;
	ExtraRestTimePerStrength = 0.0f;
	WebTraceOriginOffset = 5.0f;
	HitFlashStartTime = -10000.0f;
	MaxRestingGlobStrength = 10;
}

void AUTProj_BioLauncherShot::BeginPlay()
{
	Super::BeginPlay();

	WebReferenceAxis = ProjectileMovement->Velocity.GetSafeNormal();
}

void AUTProj_BioLauncherShot::SetGlobStrength(float NewStrength)
{
	Super::SetGlobStrength(NewStrength);

	WebHealth = GetClass()->GetDefaultObject<AUTProj_BioLauncherShot>()->WebHealth * NewStrength;
}

void AUTProj_BioLauncherShot::Landed(UPrimitiveComponent* HitComp, const FVector& HitLocation)
{
	Super::Landed(HitComp, HitLocation);

	SpawnWeb(SurfaceNormal);
}

void AUTProj_BioLauncherShot::SpawnWeb(FVector HitNormal)
{
	if (bExploded || IsPendingKill() || GetNetMode() == NM_Client || WebHitLocations.Num() > 0)
	{
		return;
	}

	float SpreadAngle = FMath::Lerp<float>(FMath::Lerp<float>(20.0f, 60.0f, FMath::Clamp<float>(GlobStrength / 10.0f, 0.0f, 1.0f)), 360.0f / FMath::Max<float>(1.0f, GlobStrength), FMath::Abs<float>(WebReferenceAxis | HitNormal));

	if ((HitNormal | FVector(0.0f, 0.0f, -1.0f)) <= 0.5f)
	{
		FVector X, Y, Z;
		FRotationMatrix(WebReferenceAxis.Rotation()).GetUnitAxes(X, Y, Z);
		HitNormal = HitNormal.RotateAngleAxis(180.f / PI * FMath::Acos(Z | HitNormal) * 0.5f, Y);
	}

	MaxWebReach = GetClass()->GetDefaultObject<AUTProj_BioLauncherShot>()->MaxWebReach * (GlobStrength / 10.0f);

	FCollisionResponseContainer WebPawnTraceResponses(ECR_Ignore);
	WebPawnTraceResponses.SetResponse(ECC_Pawn, ECR_Block);
	FCollisionShape WebPawnTraceSphere = FCollisionShape::MakeSphere(30.0f);
	TArray<FVector> SuccessfulHitDestinations;
	const int32 GlobStrengthInt = int32(GlobStrength);
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	const FVector MyLoc = GetActorLocation();
	const bool bEvenGlobStrength = GlobStrengthInt % 2 == 0;
	const int32 HalfGlobStrength = GlobStrengthInt / 2;
	for (int32 i = 0; i < GlobStrengthInt; i++)
	{
		const FVector Start = MyLoc + HitNormal * WebTraceOriginOffset;
		const bool bSecondHalf = (bEvenGlobStrength ? (i >= HalfGlobStrength) : (i > HalfGlobStrength));
		float TraceAngle;
		if (bSecondHalf)
		{
			if (bEvenGlobStrength)
			{
				TraceAngle = (i + 1 - HalfGlobStrength) * SpreadAngle - SpreadAngle * 0.5f;
			}
			else
			{
				TraceAngle = (i - HalfGlobStrength) * SpreadAngle;
			}
			TraceAngle *= -1.0f;
		}
		else
		{
			if (bEvenGlobStrength)
			{
				TraceAngle = (i + 1) * SpreadAngle - SpreadAngle * 0.5f;
			}
			else
			{
				TraceAngle = i * SpreadAngle;
			}
		}
		const FVector End = MyLoc + (HitNormal * MaxWebReach).RotateAngleAxis(TraceAngle, WebReferenceAxis);
		FHitResult Hit;
		if ( GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(FName(TEXT("BioWeb")), true)) && (Start - Hit.Location).Size() >= MinWebStringDistance &&
			!GetWorld()->SweepTestByChannel(Start, End, FQuat::Identity, ECC_Pawn, WebPawnTraceSphere, FCollisionQueryParams(FName(TEXT("BioWebPawnCheck")), true), WebPawnTraceResponses) )
		{
			SuccessfulHitDestinations.Add(End);
			WebHitLocations.Add(Hit.Location);
		}
	}

	if (SuccessfulHitDestinations.Num() == 0)
	{
		//Explode(MyLoc, HitNormal);
	}
	else
	{
		// fill out web list if necessary
		for (int32 i = 0; i < 2 && SuccessfulHitDestinations.Num() < GlobStrengthInt; i++)
		{
			for (int32 j = 0; j < SuccessfulHitDestinations.Num(); j++)
			{
				FVector HitDest = SuccessfulHitDestinations[j];
				const FVector Start = MyLoc + HitNormal * WebTraceOriginOffset;
				const FVector End = HitDest + FVector(FMath::RandRange(-200.0f, 200.0f), FMath::RandRange(-200.0f, 200.0f), FMath::RandRange(-200.0f, 200.0f));
				FHitResult Hit;
				if ( GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, FCollisionQueryParams(FName(TEXT("BioWeb")), true)) && (Start - Hit.Location).Size() >= MinWebStringDistance &&
					!GetWorld()->SweepTestByChannel(Start, End, FQuat::Identity, ECC_Pawn, WebPawnTraceSphere, FCollisionQueryParams(FName(TEXT("BioWebPawnCheck")), true), WebPawnTraceResponses) )
				{
					SuccessfulHitDestinations.Add(End);
					WebHitLocations.Add(Hit.Location);
					if (SuccessfulHitDestinations.Num() >= GlobStrengthInt)
					{
						break;
					}
				}
			}
		}

		for (const FVector& HitLoc : WebHitLocations)
		{
			UCapsuleComponent* WebCapsule = NewObject<UCapsuleComponent>(this);
			const FVector Dir = HitLoc - MyLoc;
			WebCapsule->OnComponentBeginOverlap.AddDynamic(this, &AUTProj_BioLauncherShot::OnWebOverlapBegin);
			WebCapsule->SetCollisionProfileName(TEXT("ProjectileShootable"));
			WebCapsule->SetupAttachment(RootComponent);
			WebCapsule->InitCapsuleSize(30.0f, Dir.Size() * 0.5f);
			WebCapsule->RegisterComponent();
			WebCapsule->SetWorldLocation(GetActorLocation() + Dir * 0.5f);
			WebCapsule->SetWorldRotation(FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
			if (bExploded || IsPendingKill())
			{
				// the web capsule immediately collided and blew up
				return;
			}
		}

		bSpawnedWeb = true;
		SpawnWebEffects();
	}
}

void AUTProj_BioLauncherShot::SpawnWebEffects()
{
	if (bSpawnedWeb && GetNetMode() != NM_DedicatedServer)
	{
		const FVector MyLoc = GetActorLocation();
		for (const FVector& HitLoc : WebHitLocations)
		{
			UStaticMeshComponent* WebMesh = NewObject<UStaticMeshComponent>(this);
			const FVector Dir = HitLoc - MyLoc;
			WebMesh->SetCollisionProfileName(TEXT("NoCollision"));
			WebMesh->SetStaticMesh(WebStringMesh);
			WebMesh->RelativeRotation = Dir.Rotation();
			WebMesh->RelativeRotation.Roll += 360.0f * FMath::FRand();
			WebMesh->bAbsoluteRotation = true;
			WebMesh->RelativeScale3D.X = Dir.Size() / 190.0f; // TODO: probably should use content bounds
			WebMesh->bAbsoluteScale = true;
			WebMesh->SetupAttachment(RootComponent);
			WebMesh->RegisterComponent();
			WebMIDs.Add(WebMesh->CreateAndSetMaterialInstanceDynamic(0));
		}
	}
}

float AUTProj_BioLauncherShot::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL || !GS->OnSameTeam(EventInstigator, InstigatorController))
	{
		if (!bSpawnedWeb)
		{
			if (GetNetMode() != NM_Client)
			{
				if (EventInstigator != NULL && EventInstigator->GetPawn() != NULL)
				{
					WebReferenceAxis = (EventInstigator->GetPawn()->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				}
				else if (DamageCauser != NULL)
				{
					WebReferenceAxis = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				}
				SurfaceNormal = FVector(0.0f, 0.0f, 1.0f);
				SpawnWeb(SurfaceNormal);
			}
		}
		else
		{
			WebHealth -= int32(DamageAmount);
			if (WebHealth <= 0 && GetNetMode() != NM_Client)
			{
				Explode(GetActorLocation(), SurfaceNormal);
			}
			else
			{
				WebHitFlashCount++;
				PlayWebHitFlash();
			}
		}
		return DamageAmount;
	}
	else
	{
		return 0.0f;
	}
}

void AUTProj_BioLauncherShot::PlayWebHitFlash()
{
	HitFlashStartTime = GetWorld()->TimeSeconds;
}

void AUTProj_BioLauncherShot::OnWebOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<APawn>(OtherActor) != NULL)
	{
		ProcessHit(OtherActor, OtherComp, GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
}

static FName NAME_HitFlash(TEXT("HitFlash"));
void AUTProj_BioLauncherShot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bSpawnedWeb)
	{
		const float FlashAmt = FMath::InterpEaseOut(1.0f, 0.0f, FMath::Min<float>(1.0f, (GetWorld()->TimeSeconds - HitFlashStartTime) * 2.0f), 2.0f);
		for (UMaterialInstanceDynamic* MID : WebMIDs)
		{
			if (MID != NULL)
			{
				MID->SetScalarParameterValue(NAME_HitFlash, FlashAmt);
			}
		}
	}
}

void AUTProj_BioLauncherShot::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTProj_BioLauncherShot, WebHitLocations, bSpawnedWeb);
}

void AUTProj_BioLauncherShot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_BioLauncherShot, WebHitLocations, COND_Custom);
	DOREPLIFETIME(AUTProj_BioLauncherShot, bSpawnedWeb);
	DOREPLIFETIME(AUTProj_BioLauncherShot, WebHitFlashCount);
}