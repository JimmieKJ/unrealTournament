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
	bIsCapturing = false;
	bIsPaused = false;

	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	bHasRunOnCaptureComplete = false;
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

	bHasRunOnCaptureComplete = false;
}

void AUTCTFCapturePoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFCapturePoint, bIsActive);
	DOREPLIFETIME(AUTCTFCapturePoint, CapturePercent);
	DOREPLIFETIME(AUTCTFCapturePoint, bIsPaused);
	DOREPLIFETIME(AUTCTFCapturePoint, bIsCapturing);
	DOREPLIFETIME(AUTCTFCapturePoint, bIsDraining);
	DOREPLIFETIME(AUTCTFCapturePoint, DefendersInCapsule);
	DOREPLIFETIME(AUTCTFCapturePoint, AttackersInCapsule);
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
	if (Role == ROLE_Authority)
	{
		bIsCapturing = false;
		bIsDraining = false;
		bIsPaused = false;

		DefendersInCapsule = 0;
		AttackersInCapsule = 0;

		if (bIsActive)
		{
			CalculateOccupyingCharacterCounts();
			HandleDefendingTeamSwitch();

			if (AttackersInCapsule > 0 || ((CaptureBoostPerCharacter.Num() > 0) && (CaptureBoostPerCharacter[0] > 0.f)))
			{
				if ((DefendersInCapsule == 0) || ((CaptureBoostPerCharacter.Num() > 0) && (CaptureBoostPerCharacter[0] > 0.f)))
				{
					AdvanceCapturePercent(DeltaTime);
				}
				else
				{
					bIsPaused = true;
				}
			}
			else
			{
				DecreaseCapturePercent(DeltaTime);
			}
		}
	}
	
	if (!bHasRunOnCaptureComplete && (CapturePercent >= 1.0f))
	{
		bHasRunOnCaptureComplete = true;
		OnCaptureComplete();

		if (OnCaptureCompletedDelegate.IsBound())
		{
			OnCaptureCompletedDelegate.Execute();
		}
	}

	Super::Tick(DeltaTime);
}

void AUTCTFCapturePoint::AdvanceCapturePercent(float DeltaTime)
{
	if (bIsActive)
	{
		bIsCapturing = true;

		float CaptureBoostRate = 1.f;
		if (AttackersInCapsule >= CaptureBoostPerCharacter.Num())
		{
			if (CaptureBoostPerCharacter.Num() > 0)
			{
				CaptureBoostRate = CaptureBoostPerCharacter[CaptureBoostPerCharacter.Num() - 1];
			}
		}
		else
		{
			CaptureBoostRate = CaptureBoostPerCharacter[AttackersInCapsule];
		}

		CapturePercent += ((DeltaTime / TimeToFullCapture) * CaptureBoostRate);

		if (CapturePercent >= 1.0f)
		{
			CapturePercent = 1.0f; // clamp to 1.0f
		}
	}
}

void AUTCTFCapturePoint::OnCaptureComplete_Implementation()
{
}

void AUTCTFCapturePoint::DecreaseCapturePercent(float DeltaTime)
{
	if (bIsActive)
	{
		bIsDraining = true;

		//Since the highest reached drain lock is at least 0.f because of this initialization, we are always clamping to a float > 0.
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
			bIsDraining = false;
		}
	}
}

void AUTCTFCapturePoint::HandleDefendingTeamSwitch()
{
	//Only switch defending teams if there is no current progress (IE: We are fully drained)
	if (!bIsOneSidedCapturePoint && (CapturePercent <= 0.f))
	{
		//if the attackers are in the point, and its empty. Switch team nums so that the attackers are now the defenders and vice-versa
		if ((AttackersInCapsule > 0) && (DefendersInCapsule == 0))
		{
			if (TeamNum == 0)
			{
				TeamNum = 1;
			}
			//Switch Blue to Red
			else if (TeamNum == 1)
			{
				TeamNum = 0;
			}
			//No one is on the point... set TeamNum as the no team value
			else
			{
				TeamNum = 255;
			}

			//Swap Attacker and Defender count since the TeamNum has now switched
			const int TempValue = AttackersInCapsule;
			AttackersInCapsule = DefendersInCapsule;
			DefendersInCapsule = TempValue;

			//Recalculate the number of attackers and defenders on the point since the teams have been swapped
			CalculateOccupyingCharacterCounts();
		}
	}
}

void AUTCTFCapturePoint::CalculateOccupyingCharacterCounts()
{
	for (AUTCharacter* UTChar : CharactersInCapturePoint)
	{
		if (UTChar && !UTChar->IsDead())
		{
			if (UTChar->GetTeamNum() == TeamNum)
			{
				++DefendersInCapsule;
			}
			else
			{
				++AttackersInCapsule;
			}
		}
	}
}

const TArray<AUTCharacter*>& AUTCTFCapturePoint::GetCharactersInCapturePoint()
{
	return CharactersInCapturePoint;
}

void AUTCTFCapturePoint::Reset_Implementation()
{
	CapturePercent = 0.f;

	bIsCapturing = false;
	bIsPaused = false;
	bIsDraining = false;

	DefendersInCapsule = 0;
	AttackersInCapsule = 0;

	bHasRunOnCaptureComplete = false;
}