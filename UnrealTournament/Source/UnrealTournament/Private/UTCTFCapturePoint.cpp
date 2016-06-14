// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFCapturePoint.h"
#include "Net/UnrealNetwork.h"

AUTCTFCapturePoint::AUTCTFCapturePoint(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	Capsule = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	// overlap Pawns, no other collision
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->InitCapsuleSize(92.f, 134.0f);
	Capsule->OnComponentBeginOverlap.AddDynamic(this, &AUTCTFCapturePoint::OnOverlapBegin);
	Capsule->OnComponentEndOverlap.AddDynamic(this, &AUTCTFCapturePoint::OnOverlapEnd);
	Capsule->AttachParent = RootComponent;

	CaptureBoostPerCharacter.SetNum(6, false);
	CaptureBoostPerCharacter[0] = 0.f;
	CaptureBoostPerCharacter[1] = 1.f;
	CaptureBoostPerCharacter[2] = 1.5f;
	CaptureBoostPerCharacter[3] = 2.0f;

	TimeToFullCapture = 15.f;

	bIsActive = false;
	bIsAdvancing = false;
	bIsPausedByEnemy = false;

	PrimaryActorTick.bCanEverTick = true;
}

void AUTCTFCapturePoint::BeginPlay()
{
	Super::BeginPlay();

	//ensure Drain Lock Segments are sorted from highest to lowest
	DrainLockSegments.Sort([](const float A, const float B) -> bool
	{
		if (A < B)
		{
			return false;
		}
		return true;
	});
}

void AUTCTFCapturePoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFCapturePoint, CapturePercent);
}

void AUTCTFCapturePoint::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(OtherActor);
	if (UTCharacter)
	{
		CharactersInCapturePoint.Add(UTCharacter);
	}
}

void AUTCTFCapturePoint::OnOverlapEnd(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(OtherActor);
	if (UTCharacter)
	{
		CharactersInCapturePoint.Remove(UTCharacter);
	}
}

void AUTCTFCapturePoint::Tick(float DeltaTime)
{
	bIsAdvancing = false;
	bIsDraining = false;
	bIsPausedByEnemy = false;

	TeamMatesInCapsule = 0;
	EnemiesInCapsule = 0;

	if (bIsActive)
	{
		for (AUTCharacter* UTChar : CharactersInCapturePoint)
		{
			if (UTChar)
			{
				if (UTChar->GetTeamNum() == TeamNum)
				{
					++TeamMatesInCapsule;
				}
				else
				{
					++EnemiesInCapsule;
				}
			}
		}

		if (TeamMatesInCapsule > 0 || ((CaptureBoostPerCharacter.Num() > 0) && (CaptureBoostPerCharacter[0] > 0.f)))
		{
			if ((EnemiesInCapsule == 0) || ((CaptureBoostPerCharacter.Num() > 0) && (CaptureBoostPerCharacter[0] > 0.f)))
			{
				AdvanceCapturePercent(DeltaTime);
			}
			else
			{
				bIsPausedByEnemy = true;
			}
		}
		else
		{
			AUTCTFCapturePoint::DecreaseCapturePercent(DeltaTime);
		}
	}
}

void AUTCTFCapturePoint::AdvanceCapturePercent(float DeltaTime)
{
	if (bIsActive)
	{
		bIsAdvancing = true;

		float CaptureBoostRate = 1.f;
		if (TeamMatesInCapsule > CaptureBoostPerCharacter.Num())
		{
			if (CaptureBoostPerCharacter.Num() > 0)
			{
				CaptureBoostRate = CaptureBoostPerCharacter[CaptureBoostPerCharacter.Num() - 1];
			}
		}
		else
		{
			CaptureBoostRate = CaptureBoostPerCharacter[TeamMatesInCapsule];
		}

		CapturePercent += ((DeltaTime / TimeToFullCapture) * CaptureBoostRate);

		if (CapturePercent >= 1.0f)
		{
			OnCaptureComplete();

			if (OnCaptureCompletedDelegate.IsBound())
			{
				OnCaptureCompletedDelegate.Execute();
			}
		}
	}
}

void AUTCTFCapturePoint::DecreaseCapturePercent(float DeltaTime)
{
	if (bIsActive)
	{
		bIsDraining = true;

		float HighestReachedDrainLock = 0.f;
		for (float DrainLock : DrainLockSegments)
		{
			if (CapturePercent >= DrainLock)
			{
				HighestReachedDrainLock = DrainLock;
				break;
			}
		}

		CapturePercent -= ((DeltaTime * DrainRate));

		if (CapturePercent < HighestReachedDrainLock)
		{
			CapturePercent = HighestReachedDrainLock;
		}
	}
}


void AUTCTFCapturePoint::OnCaptureComplete_Implementation()
{
	bIsActive = false;
}

const TArray<AUTCharacter*>& AUTCTFCapturePoint::GetCharactersInCapturePoint()
{
	return CharactersInCapturePoint;
}