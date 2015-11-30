// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AIModulePrivate.h"
#include "Misc/CoreMisc.h"
#include "Net/UnrealNetwork.h"
#include "NavMeshRenderingHelpers.h"
#include "DebugRenderSceneProxy.h"
#include "PerceptionGameplayDebuggerObject.h"


UPerceptionGameplayDebuggerObject::UPerceptionGameplayDebuggerObject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	LastDataCaptureTime = 0;
}


void UPerceptionGameplayDebuggerObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#if ENABLED_GAMEPLAY_DEBUGGER
	DOREPLIFETIME(UPerceptionGameplayDebuggerObject, SensingComponentLocation);
	DOREPLIFETIME(UPerceptionGameplayDebuggerObject, PerceptionLegend);
	DOREPLIFETIME(UPerceptionGameplayDebuggerObject, DistanceFromPlayer);
	DOREPLIFETIME(UPerceptionGameplayDebuggerObject, DistanceFromSensor);
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UPerceptionGameplayDebuggerObject::CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::CollectDataToReplicate(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	if (GetWorld()->TimeSeconds - LastDataCaptureTime < 2)
	{
		return;
	}

	APawn* MyPawn = Cast<APawn>(SelectedActor);
	if (MyPawn)
	{
		AAIController* BTAI = Cast<AAIController>(MyPawn->GetController());
		if (BTAI)
		{
			UAIPerceptionComponent* PerceptionComponent = BTAI->GetAIPerceptionComponent();
			if (PerceptionComponent == nullptr)
			{
				PerceptionComponent = MyPawn->FindComponentByClass<UAIPerceptionComponent>();
			}
			if (PerceptionComponent)
			{
				TArray<FString> PerceptionTexts;
				GenericShapeElements.Reset();

				PerceptionComponent->GrabGameplayDebuggerData(PerceptionTexts, GenericShapeElements);
				DistanceFromPlayer = DistanceFromSensor = -1;

				if (MyPC && MyPC->GetPawn())
				{
					DistanceFromPlayer = (MyPawn->GetActorLocation() - MyPC->GetPawn()->GetActorLocation()).Size();
					DistanceFromSensor = SensingComponentLocation != FVector::ZeroVector ? (SensingComponentLocation - MyPC->GetPawn()->GetActorLocation()).Size() : -1;
				}
			}

			UAIPerceptionSystem* PerceptionSys = UAIPerceptionSystem::GetCurrent(MyPawn->GetWorld());
			if (PerceptionSys)
			{
				PerceptionLegend = PerceptionSys->GetPerceptionDebugLegend();
			}
		}
	}
#endif
}

void UPerceptionGameplayDebuggerObject::DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::DrawCollectedData(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	PrintString(DefaultContext, FString::Printf(TEXT("Draw Colors:")));
	PrintString(DefaultContext, PerceptionLegend);

	PrintString(DefaultContext, FString::Printf(TEXT("\nDistance Sensor-PlayerPawn: %.1f\n"), DistanceFromSensor));
	PrintString(DefaultContext, FString::Printf(TEXT("Distance Pawn-PlayerPawn: %.1f\n"), DistanceFromPlayer));
#endif
}
