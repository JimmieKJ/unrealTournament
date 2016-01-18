// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AIModulePrivate.h"
#include "Misc/CoreMisc.h"
#include "Net/UnrealNetwork.h"
#include "DebugRenderSceneProxy.h"
#include "GameplayTasksComponent.h"
#include "BTGameplayDebuggerObject.h"

UBTGameplayDebuggerObject::UBTGameplayDebuggerObject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UBTGameplayDebuggerObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#if ENABLED_GAMEPLAY_DEBUGGER
	DOREPLIFETIME(UBTGameplayDebuggerObject, BrainComponentName);
	DOREPLIFETIME(UBTGameplayDebuggerObject, BrainComponentString);
#endif
}

void UBTGameplayDebuggerObject::CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::CollectDataToReplicate(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	BrainComponentName = TEXT("");
	BrainComponentString = TEXT("");

	APawn* MyPawn = Cast<APawn>(SelectedActor);
	AAIController* MyController = MyPawn ? Cast<AAIController>(MyPawn->Controller) : nullptr;
	if (MyController != NULL && MyController->BrainComponent != NULL && MyController->IsPendingKillPending() == false)
	{
		BrainComponentName = MyController->BrainComponent != NULL ? MyController->BrainComponent->GetName() : TEXT("");
		BrainComponentString = MyController->BrainComponent != NULL ? MyController->BrainComponent->GetDebugInfoString() : TEXT("");

		BlackboardString = MyController->BrainComponent->GetBlackboardComponent() ? MyController->BrainComponent->GetBlackboardComponent()->GetDebugInfoString(EBlackboardDescription::KeyWithValue) : TEXT("");

		UWorld* ActiveWorld = GetWorld();
		if (ActiveWorld && ActiveWorld->GetNetMode() != NM_Standalone)
		{
			TArray<uint8> UncompressedBuffer;
			FMemoryWriter ArWriter(UncompressedBuffer);

			ArWriter << BlackboardString;

			RequestDataReplication(TEXT("BlackboardString"), UncompressedBuffer, EGameplayDebuggerReplicate::WithoutCompression);
		}
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UBTGameplayDebuggerObject::OnDataReplicationComplited(const TArray<uint8>& ReplicatedData)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	FMemoryReader ArReader(ReplicatedData);
	ArReader << BlackboardString;
#endif
}

void UBTGameplayDebuggerObject::DrawCollectedData(APlayerController* MyPC, AActor* SelectedActor)
{
	Super::DrawCollectedData(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	if (SelectedActor)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Brain Component: {yellow}%s\n"), *BrainComponentName));
		PrintString(DefaultContext, BrainComponentString);
		PrintString(DefaultContext, FColor::White, BlackboardString, 600.0f, GetDefaultContext().DefaultY);
	}
#endif
}
