// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "ObjectEditorUtils.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "BehaviorTree/BehaviorTree.h"
#include "AIController.h"

AFunctionalAITest::AFunctionalAITest( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, CurrentSpawnSetIndex(INDEX_NONE)
	, bSingleSetRun(false)
{
	SpawnLocationRandomizationRange = 0.f;
	bWaitForNavMesh = true;
}

bool AFunctionalAITest::IsOneOfSpawnedPawns(AActor* Actor)
{
	APawn* Pawn = Cast<APawn>(Actor);
	return Pawn != NULL && SpawnedPawns.Contains(Pawn);
}

void AFunctionalAITest::BeginPlay()
{
	// do a post-load step and remove all disabled spawn sets
	for(int32 Index = SpawnSets.Num()-1; Index >= 0; --Index)
	{
		FAITestSpawnSet& SpawnSet = SpawnSets[Index];
		if (SpawnSet.bEnabled == false)
		{
			UE_LOG(LogFunctionalTest, Log, TEXT("Removing disabled spawn set \'%s\'."), *SpawnSets[Index].Name.ToString());
			SpawnSets.RemoveAt(Index, 1, false);
		}
		else
		{
			// update all spawn info that doesn't have spawn location set, and set spawn set name
			for (int32 SpawnIndex = 0; SpawnIndex < SpawnSet.SpawnInfoContainer.Num(); ++SpawnIndex)
			{
				FAITestSpawnInfo& SpawnInfo = SpawnSet.SpawnInfoContainer[SpawnIndex];
				SpawnInfo.SpawnSetName = SpawnSet.Name;
				if (SpawnInfo.SpawnLocation == NULL)
				{
					SpawnInfo.SpawnLocation = SpawnSet.FallbackSpawnLocation ? SpawnSet.FallbackSpawnLocation : this;
				}
			}
		}
	}
	SpawnSets.Shrink();

	Super::BeginPlay();
}

bool AFunctionalAITest::RunTest(const TArray<FString>& Params)
{
	KillOffSpawnedPawns();
	ClearPendingDelayedSpawns();

	RandomNumbersStream.Reset();

	bSingleSetRun = Params.Num() > 0;
	if (bSingleSetRun)
	{
		TTypeFromString<int32>::FromString(CurrentSpawnSetIndex, *Params[0]);
	}
	else
	{
		++CurrentSpawnSetIndex;
	}

	if (!SpawnSets.IsValidIndex(CurrentSpawnSetIndex))
	{
		return false;
	}
	
	StartSpawning();

	return Super::RunTest(Params);
}

void AFunctionalAITest::StartSpawning()
{
	if (bWaitForNavMesh && !IsNavMeshReady())
	{
		GetWorldTimerManager().SetTimer(NavmeshDelayTimer, this, &AFunctionalAITest::StartSpawning, 0.5f, false);
		return;
	}

	UWorld* World = GetWorld();
	check(World);
	const FAITestSpawnSet& SpawnSet = SpawnSets[CurrentSpawnSetIndex];

	bool bSuccessfullySpawnedAll = true;

	// NOTE: even if some pawns fail to spawn we don't stop spawning to find all spawns that will fails.
	// all spawned pawns get filled off in case of failure.
	CurrentSpawnSetName = SpawnSet.Name.ToString();

	for (int32 SpawnIndex = 0; SpawnIndex < SpawnSet.SpawnInfoContainer.Num(); ++SpawnIndex)
	{
		const FAITestSpawnInfo& SpawnInfo = SpawnSet.SpawnInfoContainer[SpawnIndex];
		if (SpawnInfo.IsValid())
		{
			if (SpawnInfo.PreSpawnDelay > 0)
			{
				FPendingDelayedSpawn PendingSpawnInfo(SpawnInfo);
				PendingSpawnInfo.TimeToNextSpawn = SpawnInfo.PreSpawnDelay;
				PendingSpawnInfo.NumberToSpawnLeft = SpawnInfo.NumberToSpawn;

				PendingDelayedSpawns.Add(PendingSpawnInfo);
			}
			else if (SpawnInfo.SpawnDelay == 0.0)
			{
				for (int32 SpawnedCount = 0; SpawnedCount < SpawnInfo.NumberToSpawn; ++SpawnedCount)
				{
					bSuccessfullySpawnedAll &= SpawnInfo.Spawn(this);
				}
			}
			else
			{
				bSuccessfullySpawnedAll &= SpawnInfo.Spawn(this);
				if (SpawnInfo.NumberToSpawn > 1)
				{
					PendingDelayedSpawns.Add(SpawnInfo);
				}
			}
		}
		else
		{
			const FString SpawnFailureMessage = FString::Printf(TEXT("Spawn set \'%s\' contains invalid entry at index %d")
				, *SpawnSet.Name.ToString()
				, SpawnIndex);

			UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *SpawnFailureMessage);

			bSuccessfullySpawnedAll = false;
		}
	}

	if (bSuccessfullySpawnedAll == false)
	{
		KillOffSpawnedPawns();
		
		// wait a bit if it's in the middle of StartTest call
		FTimerHandle DummyHandle;
		World->GetTimerManager().SetTimer(DummyHandle, this, &AFunctionalAITest::OnSpawningFailure, 0.1f, false);
	}		
	else
	{
		if (PendingDelayedSpawns.Num() > 0)
		{
			SetActorTickEnabled(true);
		}
	}
}

void AFunctionalAITest::OnSpawningFailure()
{
	FinishTest(EFunctionalTestResult::Failed, TEXT("Unable to spawn AI"));
}

bool AFunctionalAITest::WantsToRunAgain() const
{
	return bSingleSetRun == false && CurrentSpawnSetIndex + 1 < SpawnSets.Num();
}

void AFunctionalAITest::GatherRelevantActors(TArray<AActor*>& OutActors) const
{
	Super::GatherRelevantActors(OutActors);

	for (auto SpawnSet : SpawnSets)
	{
		if (SpawnSet.FallbackSpawnLocation)
		{
			OutActors.AddUnique(SpawnSet.FallbackSpawnLocation);
		}

		for (auto SpawnInfo : SpawnSet.SpawnInfoContainer)
		{
			if (SpawnInfo.SpawnLocation)
			{
				OutActors.AddUnique(SpawnInfo.SpawnLocation);
			}
		}
	}

	for (auto Pawn : SpawnedPawns)
	{
		if (Pawn)
		{
			OutActors.Add(Pawn);
		}
	}
}

void AFunctionalAITest::CleanUp()
{
	Super::CleanUp();
	CurrentSpawnSetIndex = INDEX_NONE;

	KillOffSpawnedPawns();
	ClearPendingDelayedSpawns();
}

FString AFunctionalAITest::GetAdditionalTestFinishedMessage(EFunctionalTestResult TestResult) const
{
	FString ResultStr;

	if (SpawnedPawns.Num() > 0)
	{
		if (CurrentSpawnSetName.Len() > 0 && CurrentSpawnSetName != TEXT("None"))
		{
			ResultStr = FString::Printf(TEXT("spawn set \'%s\', pawns: "), *CurrentSpawnSetName);
		}
		else
		{
			ResultStr = TEXT("pawns: ");
		}
		

		for (int32 PawnIndex = 0; PawnIndex < SpawnedPawns.Num(); ++PawnIndex)
		{
			ResultStr += FString::Printf(TEXT("%s, "), *GetNameSafe(SpawnedPawns[PawnIndex]));
		}
	}

	return ResultStr;
}

FString AFunctionalAITest::GetReproString() const
{
	return FString::Printf(TEXT("%s%s%d"), *(GetFName().ToString())
		, FFunctionalTesting::ReproStringParamsSeparator
		, CurrentSpawnSetIndex);
}

void AFunctionalAITest::KillOffSpawnedPawns()
{
	for (int32 PawnIndex = 0; PawnIndex < SpawnedPawns.Num(); ++PawnIndex)
	{
		if (SpawnedPawns[PawnIndex])
		{
			SpawnedPawns[PawnIndex]->Destroy();
		}
	}

	SpawnedPawns.Reset();
}

void AFunctionalAITest::ClearPendingDelayedSpawns()
{
	SetActorTickEnabled(false);
	PendingDelayedSpawns.Reset();
}

void AFunctionalAITest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (auto& DelayedSpawn : PendingDelayedSpawns)
	{
		DelayedSpawn.Tick(DeltaSeconds, this);
	}
}

void AFunctionalAITest::AddSpawnedPawn(APawn& SpawnedPawn)
{
	SpawnedPawns.Add(&SpawnedPawn);
	OnAISpawned.Broadcast(Cast<AAIController>(SpawnedPawn.GetController()), &SpawnedPawn);
}

FVector AFunctionalAITest::GetRandomizedLocation(const FVector& Location) const
{
	return Location + FVector(RandomNumbersStream.FRandRange(-SpawnLocationRandomizationRange, SpawnLocationRandomizationRange), RandomNumbersStream.FRandRange(-SpawnLocationRandomizationRange, SpawnLocationRandomizationRange), 0);
}

bool AFunctionalAITest::IsNavMeshReady() const
{
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys && NavSys->NavDataSet.Num() > 0 && !NavSys->IsNavigationBuildInProgress())
	{
		return true;
	}

	return false;
}

//----------------------------------------------------------------------//
// FAITestSpawnInfo
//----------------------------------------------------------------------//
bool FAITestSpawnInfo::Spawn(AFunctionalAITest* AITest) const
{
	check(AITest);

	bool bSuccessfullySpawned = false;

	APawn* SpawnedPawn = UAIBlueprintHelperLibrary::SpawnAIFromClass(AITest->GetWorld(), PawnClass, BehaviorTree
		, AITest->GetRandomizedLocation(SpawnLocation->GetActorLocation())
		, SpawnLocation->GetActorRotation()
		, /*bNoCollisionFail=*/true);

	if (SpawnedPawn == NULL)
	{
		FString FailureMessage = FString::Printf(TEXT("Failed to spawn \'%s\' pawn (\'%s\' set) ")
			, *GetNameSafe(PawnClass)
			, *SpawnSetName.ToString());

		UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *FailureMessage);
	}
	else if (SpawnedPawn->GetController() == NULL)
	{
		FString FailureMessage = FString::Printf(TEXT("Spawned Pawn %s (\'%s\' set) has no controller ")
			, *GetNameSafe(SpawnedPawn)
			, *SpawnSetName.ToString());

		UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *FailureMessage);
	}
	else
	{
		IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(SpawnedPawn);
		if (TeamAgent == nullptr)
		{
			TeamAgent = Cast<IGenericTeamAgentInterface>(SpawnedPawn->GetController());
		}

		if (TeamAgent != nullptr)
		{
			TeamAgent->SetGenericTeamId(TeamID);
		}

		AITest->AddSpawnedPawn(*SpawnedPawn);
		bSuccessfullySpawned = true;
	}

	return bSuccessfullySpawned;
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//

FPendingDelayedSpawn::FPendingDelayedSpawn(const FAITestSpawnInfo& Source)
	: NumberToSpawnLeft(0), bFinished(false)
{
	*((FAITestSpawnInfo*)this) = Source;
	TimeToNextSpawn = Source.SpawnDelay;
	NumberToSpawnLeft = Source.NumberToSpawn - 1;
}

void FPendingDelayedSpawn::Tick(float TimeDelta, AFunctionalAITest* AITest)
{
	if (bFinished)
	{
		return;
	}

	TimeToNextSpawn -= TimeDelta;

	if (TimeToNextSpawn <= 0)
	{
		Spawn(AITest);
		TimeToNextSpawn = SpawnDelay;
		bFinished = (--NumberToSpawnLeft <= 0);
	}
}