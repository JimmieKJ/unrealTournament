// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomBotPrivatePCH.h"
#include "UTCustomPathFollowingComponent.h"
#include "UTCustomBot.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BrainComponent.h"
#include "UTSquadAI.h"

AUTCustomBot::AUTCustomBot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTCustomPathFollowingComponent>(TEXT("PathFollowingComponent")))
{
	BlackboardKey_Enemy = MAX_uint8;
}

bool AUTCustomBot::InitializeBlackboard(UBlackboardComponent& BlackboardComp, UBlackboardData& BlackboardAsset)
{
	static const FName NAME_Enemy = TEXT("Enemy");
	static const FName NAME_EnemyVisible = TEXT("EnemyVisible");

	if (Super::InitializeBlackboard(BlackboardComp, BlackboardAsset))
	{
		BlackboardKey_Enemy = BlackboardAsset.GetKeyID(NAME_Enemy);
		BlackboardKey_EnemyVisible = BlackboardAsset.GetKeyID(NAME_EnemyVisible);

		BlackboardComponent = &BlackboardComp;

		return true;
	}

	return false;
}

void AUTCustomBot::PawnPendingDestroy(APawn* InPawn)
{
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("Pawn died"));
	}
	Super::PawnPendingDestroy(InPawn);
}

void AUTCustomBot::Possess(APawn* PossessedPawn)
{
	if (PossessedPawn)
	{
		Super::Possess(PossessedPawn);

		UAIPerceptionSystem* PercSys = UAIPerceptionSystem::GetCurrent(PossessedPawn);
		if (PercSys)
		{
			PercSys->RegisterSource<UAISense_Sight>(*PossessedPawn);
		}

		// let's use this opportunity to sing into a squad
		if (GetSquad() == nullptr)
		{
			AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (Game != nullptr)
			{
				Game->AssignDefaultSquadFor(this);
			}
			if (GetSquad() == nullptr)
			{
				UE_VLOG(this, LogUTCustomBot, Warning, TEXT("Failed to get Squad from game mode!"));
				// force default so we always have one
				SetSquad(GetWorld()->SpawnActor<AUTSquadAI>());
			}
		}

		if (BrainComponent)
		{
			BrainComponent->RestartLogic();
		}
	}
}

void AUTCustomBot::Tick(float DeltaTime)
{
	APawn* MyPawn = GetPawn();
	if (MyPawn == nullptr)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS == nullptr || PS->RespawnTime <= 0.0f)
			{
				World->GetAuthGameMode()->RestartPlayer(this);
			}
			MyPawn = GetPawn();
		}
	}

	// skip UTBot'a tick - it contains all the regular logic we want to override
	AAIController::Tick(DeltaTime);
}

FVector AUTCustomBot::GetEnemyLocation(APawn* TestEnemy, bool bAllowPrediction)
{
	if (GetAIPerceptionComponent())
	{
		const FActorPerceptionInfo* ActorInfo = GetAIPerceptionComponent()->GetActorInfo(*TestEnemy);
		return (ActorInfo) ? ActorInfo->GetLastStimulusLocation() : FVector::ZeroVector;
	}
	return TestEnemy->GetActorLocation();
}

void AUTCustomBot::ApplyWeaponAimAdjust(FVector TargetLoc, FVector& FocalPoint)
{
	FocalPoint = TargetLoc;
}

void AUTCustomBot::SetEnemy(APawn* NewEnemy)
{
	if (Enemy != NewEnemy)
	{
		if (BlackboardComponent)
		{
			BlackboardComponent->SetValue<UBlackboardKeyType_Object>(BlackboardKey_Enemy, NewEnemy);
		}

		// @hack register enemy with perception system - this won't be needed with 4.8
		if (NewEnemy)
		{
			UAIPerceptionSystem* PercSys = UAIPerceptionSystem::GetCurrent(NewEnemy);
			if (PercSys)
			{
				PercSys->RegisterSource<UAISense_Sight>(*NewEnemy);
			}
		}

		SetFocus(NewEnemy);
	}

	if (NewEnemy)
	{
		const FBotEnemyInfo* EnemyInfo = GetEnemyInfo(NewEnemy, /*bCheckTeam=*/true);
		if (EnemyInfo == nullptr)
		{
			UpdateEnemyInfo(NewEnemy, EUT_HeardApprox);
		}

		if (GetAIPerceptionComponent() && BlackboardComponent)
		{
			const FAISenseID SightSenseID = UAISense::GetSenseID<UAISense_Sight>();

			const FActorPerceptionInfo* PercInfo = GetAIPerceptionComponent()->GetActorInfo(*NewEnemy);
			if (PercInfo && PercInfo->LastSensedStimuli.IsValidIndex(SightSenseID))
			{
				BlackboardComponent->SetValue<UBlackboardKeyType_Bool>(BlackboardKey_EnemyVisible, PercInfo->LastSensedStimuli[SightSenseID].WasSuccessfullySensed());
			}
		}
	}

	Super::SetEnemy(NewEnemy);
}

//----------------------------------------------------------------------//
// new functions
//----------------------------------------------------------------------//
AUTWeapon* AUTCustomBot::GetBestWeapon() const
{
	AUTWeapon* BestWeapon = nullptr;
	float BestPriority = 0.0f;
	for (TInventoryIterator<AUTWeapon> It(Cast<AUTCharacter>(GetPawn())); It; ++It)
	{
		if (It->HasAnyAmmo())
		{
			const float TestPriority = It->AutoSwitchPriority;
			if (TestPriority > BestPriority)
			{
				BestWeapon = *It;
				BestPriority = TestPriority;
			}
		}
	}

	return BestWeapon;
}

//----------------------------------------------------------------------//
// Empty overrides
//----------------------------------------------------------------------//
void AUTCustomBot::UpdateTrackingError(bool bNewEnemy){}
void AUTCustomBot::ProcessIncomingWarning() {}