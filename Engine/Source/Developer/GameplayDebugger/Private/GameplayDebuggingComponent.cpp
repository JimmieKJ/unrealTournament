// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////
// THIS CLASS IS NOW DEPRECATED AND WILL BE REMOVED IN NEXT VERSION
// Please check GameplayDebugger.h for details.

#include "GameplayDebuggerPrivatePCH.h"
#include "Net/UnrealNetwork.h"
#include "DebugRenderSceneProxy.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavMeshRenderingComponent.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EQSRenderingComponent.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTreeDelegates.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Channel.h"
#include "Animation/AnimMontage.h"
#include "GameplayTasksComponent.h"
#include "Perception/AIPerceptionComponent.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "GeomTools.h"
#endif // WITH_EDITOR

#include "GameplayDebuggingComponent.h"
#include "GameplayDebuggingControllerComponent.h"
#include "GameplayDebuggingReplicator.h"
#include "GameplayDebuggerCategory.h"

DEFINE_LOG_CATEGORY(LogGameplayDebugger);

FGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::MakePoint(const FVector& Location, const float Radius, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShapeElement NewElement;
	NewElement.Points.Add(Location);
	NewElement.ThicknesOrRadius = Radius;
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShapeElement::SinglePoint;

	return NewElement;
}

FGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::MakeSegment(const FVector& StartLocation, const FVector& EndLocation, const float Thickness, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShapeElement NewElement;
	NewElement.Points.Add(StartLocation);
	NewElement.Points.Add(EndLocation);
	NewElement.ThicknesOrRadius = Thickness;
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShapeElement::Segment;

	return NewElement;
}

FGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::MakeBox(const FVector& Center, const FVector& Extent, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShapeElement NewElement;
	NewElement.Points.Add(Center);
	NewElement.Points.Add(Extent);
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShapeElement::Box;

	return NewElement;
}

FGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::MakeCone(const FVector& Location, const FVector& Direction, const float Length, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShapeElement NewElement;
	NewElement.Points.Add(Location);
	NewElement.Points.Add(Direction);
	NewElement.ThicknesOrRadius = Length;
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShapeElement::Cone;

	return NewElement;
}

FGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::MakeCylinder(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShapeElement NewElement;
	NewElement.Points.Add(Center - FVector(0, 0, HalfHeight));
	NewElement.Points.Add(Center + FVector(0, 0, HalfHeight));
	NewElement.ThicknesOrRadius = Radius;
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShapeElement::Cylinder;

	return NewElement;
}

FGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::MakeCapsule(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShapeElement NewElement;
	NewElement.Points.Add(Center - FVector(0, 0, HalfHeight));
	NewElement.Points.Add(Center + FVector(0, 0, HalfHeight));
	NewElement.ThicknesOrRadius = Radius;
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShapeElement::Capsule;

	return NewElement;
}

FGameplayDebuggerShapeElement FGameplayDebuggerShapeElement::MakePolygon(const TArray<FVector>& Verts, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShapeElement NewElement;
	NewElement.Points = Verts;
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShapeElement::Polygon;
	return NewElement;
}


//----------------------------------------------------------------------//
// Composite Scene proxy
//----------------------------------------------------------------------//
class FDebugRenderSceneCompositeProxy : public FDebugRenderSceneProxy
{
public:
	FDebugRenderSceneCompositeProxy(const UPrimitiveComponent* InComponent) 
		: FDebugRenderSceneProxy(InComponent)
	{
	}

	virtual ~FDebugRenderSceneCompositeProxy()
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			delete ChildProxies[Index];
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->DrawStaticElements(PDI);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		}
	}

	virtual void RegisterDebugDrawDelgate() override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->RegisterDebugDrawDelgate();
		}
	}
	
	virtual void UnregisterDebugDrawDelgate() override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->UnregisterDebugDrawDelgate();
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			Result |= ChildProxies[Index]->GetViewRelevance(View);
		}
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const override
	{
		return sizeof( *this ) + GetAllocatedSize(); 
	}

	uint32 GetAllocatedSize( void ) const
	{
		uint32 Size = 0;
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->GetMemoryFootprint();
		}

		return Size + ChildProxies.GetAllocatedSize();
	}

	void AddChild(FDebugRenderSceneProxy* NewChild)
	{
		ChildProxies.AddUnique(NewChild);
	}
	
protected:
	TArray<FDebugRenderSceneProxy*> ChildProxies;
};

//----------------------------------------------------------------------//
// UGameplayDebuggingComponent
//----------------------------------------------------------------------//

FName UGameplayDebuggingComponent::DefaultComponentName = TEXT("GameplayDebuggingComponent");
FOnDebuggingTargetChanged UGameplayDebuggingComponent::OnDebuggingTargetChangedDelegate;

UGameplayDebuggingComponent::UGameplayDebuggingComponent(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, bDrawEQSLabels(true)
	, bDrawEQSFailedItems(true)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DebugComponentClassName = TEXT("/Script/GameplayDebugger.GameplayDebuggingComponent");
		
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	bAutoActivate = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	LastStoredPathTimeStamp = -1.f;

	ShowExtendedInformatiomCounter = 0;
#if WITH_EDITOR
	bWasSelectedInEditor = false;
#endif

	bHiddenInGame = false;
	bEnabledTargetSelection = false;

	bEnableClientEQSSceneProxy = false;
	NextTargrtSelectionTime = 0;
	ReplicateViewDataCounters.Init(0, EAIDebugDrawDataView::MAX);

	AGameplayDebuggingReplicator* Replicator = Cast<AGameplayDebuggingReplicator>(GetOwner());
	if (Replicator)
	{
		Replicator->OnCycleDetailsView.AddUObject(this, &UGameplayDebuggingComponent::OnCycleDetailsView);
		FGameplayDebuggerSettings Settings = GameplayDebuggerSettings(Replicator);
		for (EAIDebugDrawDataView::Type View : TEnumRange<EAIDebugDrawDataView::Type>())
		{
			ReplicateViewDataCounters[View] = Settings.CheckFlag(View);
		}
	}
#endif
}

void UGameplayDebuggingComponent::Activate(bool bReset)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Super::Activate(bReset);
	SetComponentTickEnabled(true);
	SetIsReplicated(true);
	ActivationCounter++;
#else
	Super::Activate(bReset);
#endif
}

void UGameplayDebuggingComponent::Deactivate()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Super::Deactivate();
	SetComponentTickEnabled(false);
	SetIsReplicated(false);
#else
	Super::Deactivate();
#endif
}

void UGameplayDebuggingComponent::OnRep_ActivationCounter()
{
	// make sure that tick function is running, can't select actor to debug without it
	// if first activation (UActorComponent) comes before tick function is registered, state will be overridden by bStartWithTickEnabled (false)
	SetComponentTickEnabled(true);
}

void UGameplayDebuggingComponent::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DOREPLIFETIME(UGameplayDebuggingComponent, ReplicateViewDataCounters);
	DOREPLIFETIME(UGameplayDebuggingComponent, ShowExtendedInformatiomCounter);
	DOREPLIFETIME(UGameplayDebuggingComponent, ControllerName)
	DOREPLIFETIME(UGameplayDebuggingComponent, PawnName);
	DOREPLIFETIME(UGameplayDebuggingComponent, DebugIcon);
	DOREPLIFETIME(UGameplayDebuggingComponent, PawnClass);
	DOREPLIFETIME(UGameplayDebuggingComponent, ActivationCounter);

	DOREPLIFETIME(UGameplayDebuggingComponent, bIsUsingCharacter);
	DOREPLIFETIME(UGameplayDebuggingComponent, MovementBaseInfo);
	DOREPLIFETIME(UGameplayDebuggingComponent, MovementModeInfo);
	DOREPLIFETIME(UGameplayDebuggingComponent, MontageInfo);

	DOREPLIFETIME(UGameplayDebuggingComponent, bIsUsingPathFollowing);
	DOREPLIFETIME(UGameplayDebuggingComponent, PathFollowingInfo);
	DOREPLIFETIME(UGameplayDebuggingComponent, NavDataInfo);
	DOREPLIFETIME(UGameplayDebuggingComponent, PathPoints);
	DOREPLIFETIME(UGameplayDebuggingComponent, PathCorridorData);
	DOREPLIFETIME(UGameplayDebuggingComponent, NextPathPointIndex);

	DOREPLIFETIME(UGameplayDebuggingComponent, bIsUsingBehaviorTree);
	DOREPLIFETIME(UGameplayDebuggingComponent, CurrentAITask);
	DOREPLIFETIME(UGameplayDebuggingComponent, CurrentAIState);
	DOREPLIFETIME(UGameplayDebuggingComponent, CurrentAIAssets);

	DOREPLIFETIME(UGameplayDebuggingComponent, GameplayTasksState);

	DOREPLIFETIME(UGameplayDebuggingComponent, bIsUsingAbilities);
	DOREPLIFETIME(UGameplayDebuggingComponent, AbilityInfo);

	DOREPLIFETIME(UGameplayDebuggingComponent, BrainComponentName);
	DOREPLIFETIME(UGameplayDebuggingComponent, BrainComponentString);
	DOREPLIFETIME(UGameplayDebuggingComponent, BlackboardRepData);

	DOREPLIFETIME(UGameplayDebuggingComponent, NavmeshRepData);
	DOREPLIFETIME(UGameplayDebuggingComponent, TargetActor);
	DOREPLIFETIME(UGameplayDebuggingComponent, EQSRepData);

	DOREPLIFETIME(UGameplayDebuggingComponent, SensingComponentLocation);

	DOREPLIFETIME(UGameplayDebuggingComponent, PerceptionLegend)
	DOREPLIFETIME(UGameplayDebuggingComponent, DistanceFromPlayer);
	DOREPLIFETIME(UGameplayDebuggingComponent, DistanceFromSensor);
	DOREPLIFETIME(UGameplayDebuggingComponent, SensingComponentLocation);
//	DOREPLIFETIME(UGameplayDebuggingComponent, PerceptionShapeElements);

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UGameplayDebuggingComponent::ClientEnableTargetSelection_Validate(bool bEnable)
{
	return true;
}

void UGameplayDebuggingComponent::ClientEnableTargetSelection_Implementation(bool bEnable)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bEnabledTargetSelection = bEnable;
	UWorld* World = GetWorld();
	if (bEnabledTargetSelection && World && World->GetNetMode() != NM_DedicatedServer)
	{
		NextTargrtSelectionTime = 0;
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::SetActorToDebug(AActor* Actor)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (TargetActor != Actor)
	{
		MarkRenderStateDirty();
	}
	TargetActor = Actor;
	EQSLocalData.Reset();
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_DedicatedServer)
	{
		if (bEnabledTargetSelection)
		{
			AGameplayDebuggingReplicator* Replicator = Cast<AGameplayDebuggingReplicator>(GetOwner());
			if (Replicator)
			{
				if (Replicator->GetLocalPlayerOwner())
				{
					SelectTargetToDebug();
				}
			}
		}
	}

	if (World && World->GetNetMode() < NM_Client)
	{
		CollectDataToReplicate(true);
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::SelectTargetToDebug()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	AGameplayDebuggingReplicator* Replicator = Cast<AGameplayDebuggingReplicator>(GetOwner());

	UGameplayDebuggingControllerComponent* GDC = Replicator->FindComponentByClass<UGameplayDebuggingControllerComponent>();
	APlayerController* MyPC = GDC && GDC->GetDebugCameraController().IsValid() ? GDC->GetDebugCameraController().Get() : Replicator->GetLocalPlayerOwner();

	if (MyPC )
	{
		float bestAim = 0;
		FVector CamLocation;
		FRotator CamRotation;
		check(MyPC->PlayerCameraManager != NULL);
		MyPC->PlayerCameraManager->GetCameraViewPoint(CamLocation, CamRotation);
		FVector FireDir = CamRotation.Vector();
		UWorld* World = MyPC->GetWorld();
		check( World );

		APawn* PossibleTarget = NULL;
		for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator )
		{
			APawn* NewTarget = *Iterator;
			if (NewTarget == NULL || NewTarget == MyPC->GetPawn() 
				|| (NewTarget->PlayerState != NULL && NewTarget->PlayerState->bIsABot == false) 
				|| NewTarget->GetActorEnableCollision() == false)
			{
				continue;
			}
			
			if (NewTarget != MyPC->GetPawn())
			{
				// look for best controlled pawn target
				const FVector AimDir = NewTarget->GetActorLocation() - CamLocation;
				float FireDist = AimDir.SizeSquared();
				// only find targets which are < 25000 units away
				if (FireDist < 625000000.f)
				{
					FireDist = FMath::Sqrt(FireDist);
					float newAim = FVector::DotProduct(FireDir, AimDir);
					newAim = newAim / FireDist;
					if (newAim > bestAim)
					{
						PossibleTarget = NewTarget;
						bestAim = newAim;
					}
				}
			}
		}

		if (PossibleTarget != NULL && PossibleTarget != GetSelectedActor())
		{
			if (AGameplayDebuggingReplicator* DebuggingReplicator = Cast<AGameplayDebuggingReplicator>(GetOwner()))
			{
				DebuggingReplicator->ServerSetActorToDebug(Cast<AActor>(PossibleTarget));
				MarkRenderStateDirty();
			}

			ServerReplicateData(EDebugComponentMessage::ActivateReplication, EAIDebugDrawDataView::Empty);
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectDataToReplicate(bool bCollectExtendedData)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const AActor *SelectedActor = GetSelectedActor();
	if (!SelectedActor || SelectedActor->IsPendingKill())
	{
		return;
	}

	if (ShouldReplicateData(EAIDebugDrawDataView::Basic) || ShouldReplicateData(EAIDebugDrawDataView::OverHead))
	{
		CollectBasicData();
	}

	AGameplayDebuggingReplicator* Replicator = Cast<AGameplayDebuggingReplicator>(GetOwner());
	const bool bDrawFullData = Replicator->GetSelectedActorToDebug() == GetSelectedActor();
	if (bDrawFullData && ShouldReplicateData(EAIDebugDrawDataView::Basic))
	{
		CollectPathData();
	}

	if (bCollectExtendedData && bDrawFullData)
	{
		if (ShouldReplicateData(EAIDebugDrawDataView::BehaviorTree))
		{
			CollectBehaviorTreeData();
		}
		
#if WITH_EQS
		if (ShouldReplicateData(EAIDebugDrawDataView::EQS))
		{
			bool bEnabledEnvironmentQueryEd = true;
			if (GConfig)
			{
				GConfig->GetBool(TEXT("EnvironmentQueryEd"), TEXT("EnableEnvironmentQueryEd"), bEnabledEnvironmentQueryEd, GEngineIni);
			}

			if (bEnabledEnvironmentQueryEd)
			{
				CollectEQSData();
			}
		}
#endif // WITH_EQS
		CollectPerceptionData();
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectBasicData()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APawn* MyPawn = Cast<APawn>(GetSelectedActor());
	PawnName = MyPawn->GetHumanReadableName();
	PawnClass = MyPawn->GetClass()->GetName();
	AAIController* MyController = Cast<AAIController>(MyPawn->Controller);

	bIsUsingCharacter = MyPawn->IsA(ACharacter::StaticClass());

	if (MyController)
	{
		if (MyController->IsPendingKill() == false)
		{
			ControllerName = MyController->GetName();
			DebugIcon = MyController->GetDebugIcon();

			CollectBasicMovementData(MyPawn);
			CollectBasicPathData(MyPawn);
			CollectBasicBehaviorData(MyPawn);
			CollectBasicAbilityData(MyPawn);
			CollectBasicAnimationData(MyPawn);
		}
		else
		{
			ControllerName = TEXT("Controller PENDING KILL");
		}
	}
	else
	{
		ControllerName = TEXT("No Controller");
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectBasicMovementData(APawn* MyPawn)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UCharacterMovementComponent* CharMovement = Cast<UCharacterMovementComponent>(MyPawn->GetMovementComponent());
	if (CharMovement)
	{
		UPrimitiveComponent* FloorComponent = MyPawn->GetMovementBase();
		AActor* FloorActor = FloorComponent ? FloorComponent->GetOwner() : nullptr;
		MovementBaseInfo = FloorComponent ? FString::Printf(TEXT("%s.%s"), *GetNameSafe(FloorActor), *FloorComponent->GetName()) : TEXT("None");

		MovementModeInfo = CharMovement->GetMovementName();
	}
	else
	{
		MovementBaseInfo = TEXT("");
		MovementModeInfo = TEXT("");
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectBasicPathData(APawn* MyPawn)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(MyPawn->GetWorld());
	AAIController* MyAIController = Cast<AAIController>(MyPawn->GetController());

	const ANavigationData* NavData = NavSys ? NavSys->GetNavDataForProps(MyAIController->GetNavAgentPropertiesRef()) : nullptr;
	if (NavData)
	{
		NavDataInfo = NavData->GetConfig().Name.ToString();
	}

	UPathFollowingComponent* PFC = MyAIController->GetPathFollowingComponent();
	bIsUsingPathFollowing = (PFC != nullptr);

	if (PFC)
	{
		TArray<FString> Tokens;
		TArray<EPathFollowingDebugTokens::Type> Flags;
		if (PFC)
		{
			PFC->GetDebugStringTokens(Tokens, Flags);
		}

		PathFollowingInfo = FString();
		for (int32 Idx = 0; Idx < Tokens.Num(); Idx++)
		{
			switch (Flags[Idx])
			{
			case EPathFollowingDebugTokens::Description:
				PathFollowingInfo += Tokens[Idx];
				break;

			case EPathFollowingDebugTokens::ParamName:
				PathFollowingInfo += TEXT(", {yellow}");
				PathFollowingInfo += Tokens[Idx];
				PathFollowingInfo += TEXT(":");
				break;

			case EPathFollowingDebugTokens::PassedValue:
				PathFollowingInfo += TEXT("{yellow}");
				PathFollowingInfo += Tokens[Idx];
				break;

			case EPathFollowingDebugTokens::FailedValue:
				PathFollowingInfo += TEXT("{orange}");
				PathFollowingInfo += Tokens[Idx];
				break;

			default:
				break;
			}
		}
	}
	else
	{
		PathFollowingInfo = TEXT("");
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectBasicBehaviorData(APawn* MyPawn)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && ENABLE_VISUAL_LOG
	AAIController* MyAIController = Cast<AAIController>(MyPawn->GetController());
	UBehaviorTreeComponent* BTC = MyAIController ? Cast<UBehaviorTreeComponent>(MyAIController->BrainComponent) : nullptr;
	bIsUsingBehaviorTree = (BTC != nullptr);

	if (BTC)
	{
		CurrentAITask = BTC->DescribeActiveTasks();
		CurrentAIState = BTC->IsRunning() ? TEXT("Running") : BTC->IsPaused() ? TEXT("Paused") : TEXT("Inactive");
		CurrentAIAssets = BTC->DescribeActiveTrees();
	}
	else
	{
		CurrentAITask = TEXT("");
		CurrentAIState = TEXT("");
		CurrentAIAssets = TEXT("");
	}

	GameplayTasksState = TEXT("");
	UGameplayTasksComponent* GTComponent = MyPawn->FindComponentByClass<UGameplayTasksComponent>();
	if (GTComponent)
	{
		for (FConstGameplayTaskIterator It = GTComponent->GetPriorityQueueIterator(); It; ++It)
		{
			const UGameplayTask* QueueTask = *It;
			if (QueueTask)
			{
				const UObject* OwnerOb = Cast<const UObject>(QueueTask->GetTaskOwner());

				GameplayTasksState += FString::Printf(TEXT("{white}%s%s {%s}%s {white}Owner:{yellow}%s {white}Res:{yellow}%s\n"),
					*QueueTask->GetName(),
					QueueTask->GetInstanceName() != NAME_None ? *FString::Printf(TEXT(" {yellow}[%s]"), *QueueTask->GetInstanceName().ToString()) : TEXT(""),
					QueueTask->IsActive() ? TEXT("green") : TEXT("orange"),
					*QueueTask->GetTaskStateName(),
					(OwnerOb == GTComponent) ? TEXT("default") : *GetNameSafe(OwnerOb),
					*QueueTask->GetRequiredResources().GetDebugDescription());
			}
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectBasicAbilityData(APawn* MyPawn)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
// DEPRECATED and not supported anymore, use new category based GameplayDebugger instead
//		IGameplayAbilitiesModule& AbilitiesModule = FModuleManager::LoadModuleChecked<IGameplayAbilitiesModule>("GameplayAbilities");
//		AbilitiesModule.GetActiveAbilitiesDebugDataForActor(MyPawn, AbilityInfo, bUsingAbilities);
//		bIsUsingAbilities = bUsingAbilities;
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectBasicAnimationData(APawn* MyPawn)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ACharacter* MyChar = Cast<ACharacter>(MyPawn);
	MontageInfo = MyChar ? GetNameSafe(MyChar->GetCurrentMontage()) : TEXT("");
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectBehaviorTreeData()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!ShouldReplicateData(EAIDebugDrawDataView::BehaviorTree))
	{
		return;
	}

	BrainComponentName = TEXT("");
	BrainComponentString = TEXT("");

	APawn* MyPawn = Cast<APawn>(GetSelectedActor());
	AAIController* MyController = Cast<AAIController>(MyPawn->Controller);
	if (MyController != NULL && MyController->BrainComponent != NULL && MyController->IsPendingKillPending() == false)
	{
		BrainComponentName = MyController->BrainComponent != NULL ? MyController->BrainComponent->GetName() : TEXT("");
		BrainComponentString = MyController->BrainComponent != NULL ? MyController->BrainComponent->GetDebugInfoString() : TEXT("");

		BlackboardString = MyController->BrainComponent->GetBlackboardComponent() ? MyController->BrainComponent->GetBlackboardComponent()->GetDebugInfoString(EBlackboardDescription::KeyWithValue) : TEXT("");

		UWorld* World = GetWorld();
		if (World && World->GetNetMode() != NM_Standalone)
		{
			TArray<uint8> UncompressedBuffer;
			FMemoryWriter ArWriter(UncompressedBuffer);

			ArWriter << BlackboardString;

			const int32 HeaderSize = sizeof(int32);
			BlackboardRepData.Init(0, HeaderSize + FMath::TruncToInt(1.1f * UncompressedBuffer.Num()));

			const int32 UncompressedSize = UncompressedBuffer.Num();
			int32 CompressedSize = BlackboardRepData.Num() - HeaderSize;
			uint8* DestBuffer = BlackboardRepData.GetData();
			FMemory::Memcpy(DestBuffer, &UncompressedSize, HeaderSize);
			DestBuffer += HeaderSize;

			FCompression::CompressMemory((ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
				(void*)DestBuffer, CompressedSize, (void*)UncompressedBuffer.GetData(), UncompressedSize);

			BlackboardRepData.SetNum(CompressedSize + HeaderSize, false);
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}
void UGameplayDebuggingComponent::OnRep_UpdateBlackboard()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_Standalone)
	{
		TArray<uint8> UncompressedBuffer;
		int32 UncompressedSize = 0;
		const int32 HeaderSize = sizeof(int32);
		uint8* SrcBuffer = (uint8*)BlackboardRepData.GetData();
		FMemory::Memcpy(&UncompressedSize, SrcBuffer, HeaderSize);
		SrcBuffer += HeaderSize;
		const int32 CompressedSize = BlackboardRepData.Num() - HeaderSize;

		UncompressedBuffer.AddZeroed(UncompressedSize);

		FCompression::UncompressMemory((ECompressionFlags)(COMPRESS_ZLIB), (void*)UncompressedBuffer.GetData(), UncompressedSize, SrcBuffer, CompressedSize);
		FMemoryReader ArReader(UncompressedBuffer);

		ArReader << BlackboardString;
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::CollectPathData()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && ENABLE_VISUAL_LOG
	APawn* MyPawn = Cast<APawn>(GetSelectedActor());

	bool bRefreshRendering = false;
	if (AAIController *MyController = Cast<AAIController>(MyPawn->Controller))
	{
		if (MyController->PathFollowingComponent && MyController->PathFollowingComponent->GetPath().IsValid())
		{
			NextPathPointIndex = MyController->PathFollowingComponent->GetNextPathIndex();

			const FNavPathSharedPtr& NewPath = MyController->PathFollowingComponent->GetPath();
			if (CurrentPath.HasSameObject(NewPath.Get()) == false || NewPath->GetTimeStamp() > LastStoredPathTimeStamp)
			{
				LastStoredPathTimeStamp = NewPath->GetTimeStamp();

				FVisualLogEntry Snapshot;
				NewPath->DescribeSelfToVisLog(&Snapshot);
				PathCorridorPolygons.Reset(Snapshot.ElementsToDraw.Num());
				FPathCorridorPolygons Polygon;
				for (FVisualLogShapeElement& CurrentShape : Snapshot.ElementsToDraw)
				{
					if (CurrentShape.GetType() == EVisualLoggerShapeElement::Polygon)
					{
						Polygon.Color = CurrentShape.GetFColor();
						Polygon.Points.Reset();
						Polygon.Points.Append(CurrentShape.Points);
						PathCorridorPolygons.Add(Polygon);
					}
				}

				PathPoints.Reset();
				for (int32 Index=0; Index < NewPath->GetPathPoints().Num(); ++Index)
				{
					PathPoints.Add(NewPath->GetPathPoints()[Index].Location);
				}
				CurrentPath = NewPath;

				UWorld* World = GetWorld();
				if (PathCorridorPolygons.Num() && World && World->GetNetMode() != NM_Client)
				{
					PathCorridorData.Reset();
					TArray<uint8> HelpBuffer;
					FMemoryWriter ArWriter(HelpBuffer);
					int32 NumPolygons = PathCorridorPolygons.Num();
					ArWriter << NumPolygons;
					for (const FPathCorridorPolygons& CurrentPoly : PathCorridorPolygons)
					{
						FColor Color = CurrentPoly.Color;
						ArWriter << Color;
						int32 NumVerts = CurrentPoly.Points.Num();
						ArWriter << NumVerts;
						for (auto Vert : CurrentPoly.Points)
						{
							ArWriter << Vert;
						}
					}
					PathCorridorData = HelpBuffer;
				}
				bRefreshRendering = true;
			}
		}
		else
		{
			NextPathPointIndex = INDEX_NONE;
			CurrentPath = NULL;
			PathPoints.Reset();
			PathCorridorPolygons.Reset();
		}
	}

	UWorld* World = GetWorld();
	if (bRefreshRendering && World && World->GetNetMode() != NM_DedicatedServer)
	{
		UpdateBounds();
		MarkRenderStateDirty();
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::OnRep_PathCorridorData()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_DedicatedServer)
	{
		FMemoryReader ArReader(PathCorridorData);
		int32 NumPolygons = 0;
		ArReader << NumPolygons;
		PathCorridorPolygons.Reset(NumPolygons);
		for (int32 PolyIndex = 0; PolyIndex < NumPolygons; ++PolyIndex)
		{
			FPathCorridorPolygons Polygon;
			FColor Color;
			ArReader << Polygon.Color;

			int32 NumVerts = 0;
			ArReader << NumVerts;
			for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
			{
				FVector CurrentVertex;
				ArReader << CurrentVertex;
				Polygon.Points.Add(CurrentVertex);
			}
			if (NumVerts > 2)
			{
				PathCorridorPolygons.Add(Polygon);
			}
		}
		UpdateBounds();
		MarkRenderStateDirty();
	}
#endif
}


void UGameplayDebuggingComponent::EnableDebugDraw(bool bEnable, bool InFocusedComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
#if WITH_EQS
	EnableClientEQSSceneProxy(bEnable);
#endif // WITH_EQS
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingComponent::ServerReplicateData(uint32 InMessage, uint32  InDataView)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	EDebugComponentMessage::Type Message = (EDebugComponentMessage::Type)InMessage;
	EAIDebugDrawDataView::Type DataView = (EAIDebugDrawDataView::Type) InDataView;
	switch (Message)
	{
	case EDebugComponentMessage::EnableExtendedView:
		ShowExtendedInformatiomCounter++;
		break;

	case EDebugComponentMessage::DisableExtendedView:
		ShowExtendedInformatiomCounter = FMath::Max(0, ShowExtendedInformatiomCounter - 1);
		break;

	case EDebugComponentMessage::ActivateReplication:
		Activate();
		break;

	case EDebugComponentMessage::DeactivateReplilcation:
		Deactivate();
		break;

	case EDebugComponentMessage::ActivateDataView:
		if (ReplicateViewDataCounters.IsValidIndex(DataView))
		{
			ReplicateViewDataCounters[DataView] += 1;
		}
		break;

	case EDebugComponentMessage::DeactivateDataView:
		if (ReplicateViewDataCounters.IsValidIndex(DataView))
		{
			ReplicateViewDataCounters[DataView] = FMath::Max(0, ReplicateViewDataCounters[DataView] - 1);
		}
		break;

	default:
		break;
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

//////////////////////////////////////////////////////////////////////////
// EQS Data
//////////////////////////////////////////////////////////////////////////
void UGameplayDebuggingComponent::OnCycleDetailsView()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (++CurrentEQSIndex >= EQSLocalData.Num())
	{
		CurrentEQSIndex = 0;
	}

	UpdateBounds();
	MarkRenderStateDirty();
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

const FEnvQueryResult* UGameplayDebuggingComponent::GetQueryResult() const
{
	return CachedQueryInstance.Get();
}

const FEnvQueryInstance* UGameplayDebuggingComponent::GetQueryInstance() const
{
	return CachedQueryInstance.Get();
}

void UGameplayDebuggingComponent::OnRep_UpdateEQS()
{
#if  USE_EQS_DEBUGGER
	// decode scoring data
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_Client)
	{
		TArray<uint8> UncompressedBuffer;
		int32 UncompressedSize = 0;
		const int32 HeaderSize = sizeof(int32);
		uint8* SrcBuffer = (uint8*)EQSRepData.GetData();
		FMemory::Memcpy(&UncompressedSize, SrcBuffer, HeaderSize);
		SrcBuffer += HeaderSize;
		const int32 CompressedSize = EQSRepData.Num() - HeaderSize;

		UncompressedBuffer.AddZeroed(UncompressedSize);

		FCompression::UncompressMemory((ECompressionFlags)(COMPRESS_ZLIB), (void*)UncompressedBuffer.GetData(), UncompressedSize, SrcBuffer, CompressedSize);
		FMemoryReader ArReader(UncompressedBuffer);

		ArReader << EQSLocalData;
	}	

	UpdateBounds();
	MarkRenderStateDirty();
#endif //USE_EQS_DEBUGGER
}

void UGameplayDebuggingComponent::CollectEQSData()
{
#if USE_EQS_DEBUGGER
	if (!ShouldReplicateData(EAIDebugDrawDataView::EQS))
	{
		return;
	}

	UWorld* World = GetWorld();
	UEnvQueryManager* QueryManager = World ? UEnvQueryManager::GetCurrent(World) : NULL;
	const AActor* Owner = GetSelectedActor();
	AGameplayDebuggingReplicator* Replicator = Cast<AGameplayDebuggingReplicator>(GetOwner());

	if (QueryManager == NULL || Owner == NULL)
	{
		return;
	}

	auto AllQueries = QueryManager->GetDebugger().GetAllQueriesForOwner(Owner);
	const class APawn* OwnerAsPawn = Cast<class APawn>(Owner);
	if (OwnerAsPawn != NULL && OwnerAsPawn->GetController())
	{
		const auto& AllControllerQueries = QueryManager->GetDebugger().GetAllQueriesForOwner(OwnerAsPawn->GetController());
		AllQueries.Append(AllControllerQueries);
	}
	struct FEnvQueryInfoSort
	{
		FORCEINLINE bool operator()(const FEQSDebugger::FEnvQueryInfo& A, const FEQSDebugger::FEnvQueryInfo& B) const
		{
			return (A.Timestamp < B.Timestamp);
		}
	};
	TArray<FEQSDebugger::FEnvQueryInfo> QueriesToSort = AllQueries;
	QueriesToSort.Sort(FEnvQueryInfoSort()); //sort queries by timestamp
	QueriesToSort.SetNum(FMath::Min<int32>(Replicator->MaxEQSQueries, AllQueries.Num()));

	for (int32 Index = AllQueries.Num() - 1; Index >= 0; --Index)
	{
		auto &CurrentQuery = AllQueries[Index];
		if (QueriesToSort.Find(CurrentQuery) == INDEX_NONE)
		{
			AllQueries.RemoveAt(Index);
		}
	}


	EQSLocalData.Reset();
	for (int32 Index = 0; Index < FMath::Min<int32>(Replicator->MaxEQSQueries, AllQueries.Num()); ++Index)
	{
		EQSDebug::FQueryData* CurrentLocalData = NULL;
		CachedQueryInstance = AllQueries[Index].Instance;
		const float CachedTimestamp = AllQueries[Index].Timestamp;

		if (!CurrentLocalData)
		{
			EQSLocalData.AddZeroed();
			CurrentLocalData = &EQSLocalData[EQSLocalData.Num()-1];
		}

		if (CachedQueryInstance.IsValid())
		{
			UEnvQueryDebugHelpers::QueryToDebugData(*CachedQueryInstance, *CurrentLocalData);
		}
		CurrentLocalData->Timestamp = AllQueries[Index].Timestamp;
	}

	TArray<uint8> UncompressedBuffer;
	FMemoryWriter ArWriter(UncompressedBuffer);

	ArWriter << EQSLocalData;

	const int32 UncompressedSize = UncompressedBuffer.Num();
	const int32 HeaderSize = sizeof(int32);
	EQSRepData.Init(0, HeaderSize + FMath::TruncToInt(1.1f * UncompressedSize));

	int32 CompressedSize = EQSRepData.Num() - HeaderSize;
	uint8* DestBuffer = EQSRepData.GetData();
	FMemory::Memcpy(DestBuffer, &UncompressedSize, HeaderSize);
	DestBuffer += HeaderSize;

	FCompression::CompressMemory((ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory), (void*)DestBuffer, CompressedSize, (void*)UncompressedBuffer.GetData(), UncompressedSize);

	EQSRepData.SetNum(CompressedSize + HeaderSize, false);

	if (World && World->GetNetMode() != NM_DedicatedServer)
	{
		OnRep_UpdateEQS();
	}
#endif
}

//----------------------------------------------------------------------//
// NavMesh rendering
//----------------------------------------------------------------------//
void UGameplayDebuggingComponent::OnRep_UpdateNavmesh()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	NavMeshBounds = FBox(FVector(-HALF_WORLD_MAX1, -HALF_WORLD_MAX1, -HALF_WORLD_MAX1), FVector(HALF_WORLD_MAX1, HALF_WORLD_MAX1, HALF_WORLD_MAX1));
	UpdateBounds();
	MarkRenderStateDirty();
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UGameplayDebuggingComponent::ServerCollectNavmeshData_Validate(FVector_NetQuantize10 TargetLocation)
{
	bool bIsValid = false;
#if WITH_RECAST
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	bIsValid = NavSys && NavSys->GetMainNavData(FNavigationSystem::DontCreate);
#endif

	return bIsValid;
}

bool UGameplayDebuggingComponent::ServerDiscardNavmeshData_Validate()
{
	return true;
}

namespace NavMeshDebug
{
	struct FShortVector
	{
		int16 X;
		int16 Y;
		int16 Z;

		FShortVector()
			:X(0), Y(0), Z(0)
		{

		}

		FShortVector(const FVector& Vec)
			: X(Vec.X)
			, Y(Vec.Y)
			, Z(Vec.Z)
		{

		}
		FShortVector& operator=(const FVector& R)
		{
			X = R.X;
			Y = R.Y;
			Z = R.Z;
			return *this;
		}

		FVector ToVector() const
		{
			return FVector(X, Y, Z);
		}
	};

	struct FOffMeshLinkFlags
	{
		uint8 Radius : 6;
		uint8 Direction : 1;
		uint8 ValidEnds : 1;
	};
	struct FOffMeshLink
	{
		FShortVector Left;
		FShortVector Right;
		FColor Color;
		union
		{
			FOffMeshLinkFlags PackedFlags;
			uint8 ByteFlags;
		};
	};

	struct FAreaPolys
	{
		TArray<int32> Indices;
		FColor Color;
	};

	struct FTileData
	{
		FVector Location;
		TArray<FAreaPolys> Areas;
		TArray<FShortVector> Verts;
		TArray<FOffMeshLink> Links;
	};
}

FArchive& operator<<(FArchive& Ar, NavMeshDebug::FShortVector& ShortVector)
{
	Ar << ShortVector.X;
	Ar << ShortVector.Y;
	Ar << ShortVector.Z;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, NavMeshDebug::FOffMeshLink& Data)
{
	Ar << Data.Left;
	Ar << Data.Right;
	Ar << Data.Color.R;
	Ar << Data.Color.G;
	Ar << Data.Color.B;
	Ar << Data.ByteFlags;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, NavMeshDebug::FAreaPolys& Data)
{
	Ar << Data.Indices;
	Ar << Data.Color.R;
	Ar << Data.Color.G;
	Ar << Data.Color.B;
	Data.Color.A = 255;
	return Ar;
}


FArchive& operator<<(FArchive& Ar, NavMeshDebug::FTileData& Data)
{
	Ar << Data.Areas;
	Ar << Data.Location;
	Ar << Data.Verts;
	Ar << Data.Links;
	return Ar;
}

void UGameplayDebuggingComponent::ServerDiscardNavmeshData_Implementation()
{
	NavmeshRepData.Empty();
}

FORCEINLINE bool LineInCorrectDistance(const FVector& PlayerLoc, const FVector& Start, const FVector& End, float CorrectDistance = -1)
{
	const float MaxDistance = CorrectDistance > 0 ? (CorrectDistance*CorrectDistance) : ARecastNavMesh::GetDrawDistanceSq();

	if ((FVector::DistSquared(Start, PlayerLoc) > MaxDistance || FVector::DistSquared(End, PlayerLoc) > MaxDistance))
	{
		return false;
	}
	return true;
}

#if WITH_RECAST
ARecastNavMesh* UGameplayDebuggingComponent::GetNavData()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys == NULL)
	{
		return NULL;
	}

	// Try to get the correct nav-mesh relative to the selected actor.
	APawn* TargetPawn = Cast<APawn>(TargetActor);
	if (TargetPawn != NULL)
	{
		const FNavAgentProperties& NavAgentProperties = TargetPawn->GetNavAgentPropertiesRef();
		return Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(NavAgentProperties));
	}

	// If it wasn't found, just get the main nav-mesh data.
	return Cast<ARecastNavMesh>(NavSys->GetMainNavData(FNavigationSystem::DontCreate));
#else
	return NULL;
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}
#endif

static const FColor NavMeshRenderColor_Recast_TriangleEdges(255, 255, 255);
static const FColor NavMeshRenderColor_Recast_TileEdges(16, 16, 16, 32);
static const FColor NavMeshRenderColor_Recast_NavMeshEdges(32, 63, 0, 220);
static const FColor NavMeshRenderColor_RecastMesh(140, 255, 0, 164);
static const FColor NavMeshRenderColor_TileBounds(255, 255, 64, 255);
static const FColor NavMeshRenderColor_PathCollidingGeom(255, 255, 255, 40);
static const FColor NavMeshRenderColor_RecastTileBeingRebuilt(255, 0, 0, 64);
static const FColor NavMeshRenderColor_OffMeshConnectionInvalid(64, 64, 64);
static const float DefaultEdges_LineThickness = 0.0f;
static const float PolyEdges_LineThickness = 1.5f;
static const float NavMeshEdges_LineThickness = 3.5f;
static const float LinkLines_LineThickness = 2.0f;
static const float ClusterLinkLines_LineThickness = 2.0f;

namespace FNavMeshRenderingHelpers_DEPRECATEDSUPPORT
{
	bool LineInView(const FVector& Start, const FVector& End, const FSceneView* View, bool bUseDistanceCheck)
	{
		if (FVector::DistSquared(Start, View->ViewMatrices.ViewOrigin) > ARecastNavMesh::GetDrawDistanceSq() ||
			FVector::DistSquared(End, View->ViewMatrices.ViewOrigin) > ARecastNavMesh::GetDrawDistanceSq())
		{
			return false;
		}

		for (int32 PlaneIdx = 0; PlaneIdx < View->ViewFrustum.Planes.Num(); ++PlaneIdx)
		{
			const FPlane& CurPlane = View->ViewFrustum.Planes[PlaneIdx];
			if (CurPlane.PlaneDot(Start) > 0.f && CurPlane.PlaneDot(End) > 0.f)
			{
				return false;
			}
		}

		return true;
	}

	bool LineInCorrectDistance(const FVector& Start, const FVector& End, const FSceneView* View, float CorrectDistance = -1)
	{
		const float MaxDistanceSq = (CorrectDistance > 0) ? FMath::Square(CorrectDistance) : ARecastNavMesh::GetDrawDistanceSq();
		return FVector::DistSquared(Start, View->ViewMatrices.ViewOrigin) < MaxDistanceSq &&
			FVector::DistSquared(End, View->ViewMatrices.ViewOrigin) < MaxDistanceSq;
	}

	FVector EvalArc(const FVector& Org, const FVector& Dir, const float h, const float u)
	{
		FVector Pt = Org + Dir * u;
		Pt.Z += h * (1 - (u * 2 - 1)*(u * 2 - 1));

		return Pt;
	}

	void CacheArc(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Start, const FVector& End, const float Height, const uint32 Segments, const FLinearColor& Color, float LineThickness = 0)
	{
		if (Segments == 0)
		{
			return;
		}

		const float ArcPtsScale = 1.0f / (float)Segments;
		const FVector Dir = End - Start;
		const float Length = Dir.Size();

		FVector Prev = Start;
		for (uint32 i = 1; i <= Segments; ++i)
		{
			const float u = i * ArcPtsScale;
			const FVector Pt = EvalArc(Start, Dir, Length*Height, u);

			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Prev, Pt, Color.ToFColor(true)));
			Prev = Pt;
		}
	}

	void CacheArrowHead(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Tip, const FVector& Origin, const float Size, const FLinearColor& Color, float LineThickness = 0)
	{
		FVector Ax, Ay, Az(0, 1, 0);
		Ay = Origin - Tip;
		Ay.Normalize();
		Ax = FVector::CrossProduct(Az, Ay);

		FHitProxyId HitProxyId;
		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Tip, FVector(Tip.X + Ay.X*Size + Ax.X*Size / 3, Tip.Y + Ay.Y*Size + Ax.Y*Size / 3, Tip.Z + Ay.Z*Size + Ax.Z*Size / 3), Color.ToFColor(true)));
		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Tip, FVector(Tip.X + Ay.X*Size - Ax.X*Size / 3, Tip.Y + Ay.Y*Size - Ax.Y*Size / 3, Tip.Z + Ay.Z*Size - Ax.Z*Size / 3), Color.ToFColor(true)));
	}

	void DrawWireCylinder(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, FColor Color, float Radius, float HalfHeight, int32 NumSides, uint8 DepthPriority, float LineThickness = 0)
	{
		const float	AngleDelta = 2.0f * PI / NumSides;
		FVector	LastVertex = Base + X * Radius;

		FHitProxyId HitProxyId;
		for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
		{
			const FVector Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;

			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex - Z * HalfHeight, Vertex - Z * HalfHeight, Color));
			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex + Z * HalfHeight, Vertex + Z * HalfHeight, Color));
			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex - Z * HalfHeight, LastVertex + Z * HalfHeight, Color));

			LastVertex = Vertex;
		}
	}

	inline uint8 GetBit(int32 v, uint8 bit)
	{
		return (v & (1 << bit)) >> bit;
	}

	FColor GetClusterColor(int32 Idx)
	{
		uint8 r = 1 + GetBit(Idx, 1) + GetBit(Idx, 3) * 2;
		uint8 g = 1 + GetBit(Idx, 2) + GetBit(Idx, 4) * 2;
		uint8 b = 1 + GetBit(Idx, 0) + GetBit(Idx, 5) * 2;
		return FColor(r * 63, g * 63, b * 63, 164);
	}

	FColor DarkenColor(const FColor& Base)
	{
		const uint32 Col = Base.DWColor();
		return FColor(((Col >> 1) & 0x007f7f7f) | (Col & 0xff000000));
	}

	void AddVertex(FNavMeshSceneProxyData::FDebugMeshData& MeshData, const FVector& Pos, const FColor Color = FColor::White)
	{
		const int32 VertexIndex = MeshData.Vertices.Num();
		FDynamicMeshVertex* Vertex = new(MeshData.Vertices) FDynamicMeshVertex;
		Vertex->Position = Pos;
		Vertex->TextureCoordinate = FVector2D::ZeroVector;
		Vertex->TangentX = FVector(1.0f, 0.0f, 0.0f);
		Vertex->TangentZ = FVector(0.0f, 1.0f, 0.0f);
		// store the sign of the determinant in TangentZ.W (-1=0,+1=255)
		Vertex->TangentZ.Vector.W = 255;
		Vertex->Color = Color;
	}

	void AddTriangle(FNavMeshSceneProxyData::FDebugMeshData& MeshData, int32 V0, int32 V1, int32 V2)
	{
		MeshData.Indices.Add(V0);
		MeshData.Indices.Add(V1);
		MeshData.Indices.Add(V2);
	}

	FVector Recast2UnrealPoint(const float* RecastPoint)
	{
		return FVector(-RecastPoint[0], -RecastPoint[2], RecastPoint[1]);
	}

	void AddRecastGeometry(TArray<FVector>& OutVertexBuffer, TArray<int32>& OutIndexBuffer, const float* Coords, int32 NumVerts, const int32* Faces, int32 NumFaces)
	{
		const int32 VertIndexBase = OutVertexBuffer.Num();
		for (int32 VertIdx = 0; VertIdx < NumVerts * 3; VertIdx += 3)
		{
			OutVertexBuffer.Add(Recast2UnrealPoint(&Coords[VertIdx]));
		}

		const int32 FirstNewFaceVertexIndex = OutIndexBuffer.Num();
		const uint32 NumIndices = NumFaces * 3;
		OutIndexBuffer.AddUninitialized(NumIndices);
		for (uint32 Index = 0; Index < NumIndices; ++Index)
		{
			OutIndexBuffer[FirstNewFaceVertexIndex + Index] = VertIndexBase + Faces[Index];
		}
	}
}


void UGameplayDebuggingComponent::ServerCollectNavmeshData_Implementation(FVector_NetQuantize10 TargetLocation)
{
#if WITH_RECAST
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	ARecastNavMesh* NavData = GetNavData();
	if (NavData == NULL)
	{
		NavmeshRepData.Empty();
		return;
	}

	const double Timer1 = FPlatformTime::Seconds();

	TArray<int32> Indices;
	int32 TileX = 0;
	int32 TileY = 0;
	const int32 DeltaX[] = { 0, 1, 1, 0, -1, -1, -1,  0,  1 };
	const int32 DeltaY[] = { 0, 0, 1, 1,  1,  0, -1, -1, -1 };

	NavData->BeginBatchQuery();

	// add 3x3 neighborhood of target
	int32 TargetTileX = 0;
	int32 TargetTileY = 0;
	NavData->GetNavMeshTileXY(TargetLocation, TargetTileX, TargetTileY);
	for (int32 i = 0; i < ARRAY_COUNT(DeltaX); i++)
	{
		const int32 NeiX = TargetTileX + DeltaX[i];
		const int32 NeiY = TargetTileY + DeltaY[i];

		NavData->GetNavMeshTilesAt(NeiX, NeiY, Indices);
	}

	const FNavDataConfig& NavConfig = NavData->GetConfig();
	FColor NavMeshColors[RECAST_MAX_AREAS];
	NavMeshColors[RECAST_DEFAULT_AREA] = NavConfig.Color.DWColor() > 0 ? NavConfig.Color : NavMeshRenderColor_RecastMesh;
	for (uint8 i = 0; i < RECAST_DEFAULT_AREA; ++i)
	{
		NavMeshColors[i] = NavData->GetAreaIDColor(i);
	}

	TArray<uint8> UncompressedBuffer;
	FMemoryWriter ArWriter(UncompressedBuffer);

	int32 NumIndices = Indices.Num();
	ArWriter << NumIndices;

	for (int32 i = 0; i < NumIndices; i++)
	{
		FRecastDebugGeometry NavMeshGeometry;
		NavMeshGeometry.bGatherPolyEdges = false;
		NavMeshGeometry.bGatherNavMeshEdges = false;
		NavData->GetDebugGeometry(NavMeshGeometry, Indices[i]);

		NavMeshDebug::FTileData TileData;
		const FBox TileBoundingBox = NavData->GetNavMeshTileBounds(Indices[i]);
		TileData.Location = TileBoundingBox.GetCenter();
		for (int32 VertIndex = 0; VertIndex < NavMeshGeometry.MeshVerts.Num(); ++VertIndex)
		{
			const NavMeshDebug::FShortVector SV = NavMeshGeometry.MeshVerts[VertIndex] - TileData.Location;
			TileData.Verts.Add(SV);
		}

		for (int32 iArea = 0; iArea < RECAST_MAX_AREAS; iArea++)
		{
			const int32 NumTris = NavMeshGeometry.AreaIndices[iArea].Num();
			if (NumTris)
			{
				NavMeshDebug::FAreaPolys AreaPolys;
				for (int32 AreaIndicesIndex = 0; AreaIndicesIndex < NavMeshGeometry.AreaIndices[iArea].Num(); ++AreaIndicesIndex)
				{
					AreaPolys.Indices.Add(NavMeshGeometry.AreaIndices[iArea][AreaIndicesIndex]);
				}
				AreaPolys.Color = NavMeshColors[iArea];
				TileData.Areas.Add(AreaPolys);
			}
		}

		TileData.Links.Reserve(NavMeshGeometry.OffMeshLinks.Num());
		for (int32 iLink = 0; iLink < NavMeshGeometry.OffMeshLinks.Num(); iLink++)
		{
			const FRecastDebugGeometry::FOffMeshLink& SrcLink = NavMeshGeometry.OffMeshLinks[iLink];
			NavMeshDebug::FOffMeshLink Link;
			Link.Left = SrcLink.Left - TileData.Location;
			Link.Right = SrcLink.Right - TileData.Location;
			Link.Color = ((SrcLink.Direction && SrcLink.ValidEnds) || (SrcLink.ValidEnds & FRecastDebugGeometry::OMLE_Left)) ? NavMeshColors[SrcLink.AreaID] : NavMeshRenderColor_OffMeshConnectionInvalid;
			Link.PackedFlags.Radius = (int8)SrcLink.Radius;
			Link.PackedFlags.Direction = SrcLink.Direction;
			Link.PackedFlags.ValidEnds = SrcLink.ValidEnds;
			TileData.Links.Add(Link);
		}

		ArWriter << TileData;
	}
	NavData->FinishBatchQuery();

	const double Timer2 = FPlatformTime::Seconds();

	const int32 HeaderSize = sizeof(int32);
	NavmeshRepData.Init(0, HeaderSize + FMath::TruncToInt(1.1f * UncompressedBuffer.Num()));

	const int32 UncompressedSize = UncompressedBuffer.Num();
	int32 CompressedSize = NavmeshRepData.Num() - HeaderSize;
	uint8* DestBuffer = NavmeshRepData.GetData();
	FMemory::Memcpy(DestBuffer, &UncompressedSize, HeaderSize);
	DestBuffer += HeaderSize;

	FCompression::CompressMemory((ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
		(void*)DestBuffer, CompressedSize, (void*)UncompressedBuffer.GetData(), UncompressedSize);

	NavmeshRepData.SetNum(CompressedSize + HeaderSize, false);

	const double Timer3 = FPlatformTime::Seconds();
	UE_LOG(LogGameplayDebugger, Log, TEXT("Preparing navmesh data: %.1fkB took %.3fms (collect: %.3fms + compress %d%%: %.3fms)"),
		NavmeshRepData.Num() / 1024.0f, 1000.0f * (Timer3 - Timer1),
		1000.0f * (Timer2 - Timer1),
		FMath::TruncToInt(100.0f * NavmeshRepData.Num() / UncompressedBuffer.Num()), 1000.0f * (Timer3 - Timer2));
#endif

	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_DedicatedServer)
	{
		OnRep_UpdateNavmesh();
	}
}

void UGameplayDebuggingComponent::PrepareNavMeshData(struct FNavMeshSceneProxyData* CurrentData) const
{
#if WITH_RECAST
	if (CurrentData)
	{
		CurrentData->Reset();
		CurrentData->bNeedsNewData = false;

		// uncompress data
		TArray<uint8> UncompressedBuffer;
		const int32 HeaderSize = sizeof(int32);
		if (NavmeshRepData.Num() > HeaderSize)
		{
			int32 UncompressedSize = 0;
			uint8* SrcBuffer = (uint8*)NavmeshRepData.GetData();
			FMemory::Memcpy(&UncompressedSize, SrcBuffer, HeaderSize);
			SrcBuffer += HeaderSize;
			const int32 CompressedSize = NavmeshRepData.Num() - HeaderSize;

			UncompressedBuffer.AddZeroed(UncompressedSize);

			FCompression::UncompressMemory((ECompressionFlags)(COMPRESS_ZLIB),
				(void*)UncompressedBuffer.GetData(), UncompressedSize, SrcBuffer, CompressedSize);
		}

		// read serialized values
		if (UncompressedBuffer.Num() == 0)
		{
			return;
		}

		FMemoryReader ArReader(UncompressedBuffer);
		int32 NumTiles = 0;
		ArReader << NumTiles;

		int32 IndexesOffset = 0;
		for (int32 iTile = 0; iTile < NumTiles; iTile++)
		{
			NavMeshDebug::FTileData TileData;
			ArReader << TileData;

			FVector OffsetLocation = TileData.Location;
			TArray<FVector> Verts; 
			Verts.Reserve(TileData.Verts.Num());
			for (int32 VertIndex = 0; VertIndex < TileData.Verts.Num(); ++VertIndex)
			{
				const FVector Loc = TileData.Verts[VertIndex].ToVector() + OffsetLocation;
				Verts.Add(Loc);
			}
			CurrentData->Bounds += FBox(Verts);
			
			for (int32 iArea = 0; iArea < TileData.Areas.Num(); iArea++)
			{
				const NavMeshDebug::FAreaPolys& SrcArea = TileData.Areas[iArea];
				FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
				DebugMeshData.ClusterColor = SrcArea.Color;
				DebugMeshData.ClusterColor.A = 128;
				
				for (int32 iTri = 0; iTri < SrcArea.Indices.Num(); iTri += 3)
				{
					const int32 Index0 = SrcArea.Indices[iTri + 0];
					const int32 Index1 = SrcArea.Indices[iTri + 1];
					const int32 Index2 = SrcArea.Indices[iTri + 2];

					FVector V0 = Verts[Index0];
					FVector V1 = Verts[Index1];
					FVector V2 = Verts[Index2];
					CurrentData->TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(V0 + CurrentData->NavMeshDrawOffset, V1 + CurrentData->NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));
					CurrentData->TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(V1 + CurrentData->NavMeshDrawOffset, V2 + CurrentData->NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));
					CurrentData->TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(V2 + CurrentData->NavMeshDrawOffset, V0 + CurrentData->NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));

					FNavMeshRenderingHelpers_DEPRECATEDSUPPORT::AddTriangle(DebugMeshData, Index0 + IndexesOffset, Index1 + IndexesOffset, Index2 + IndexesOffset);
				}

				for (int32 iVert = 0; iVert < Verts.Num(); iVert++)
				{
					FNavMeshRenderingHelpers_DEPRECATEDSUPPORT::AddVertex(DebugMeshData, Verts[iVert] + CurrentData->NavMeshDrawOffset);
				}
				CurrentData->MeshBuilders.Add(DebugMeshData);
				IndexesOffset += Verts.Num();
			}

			for (int32 i = 0; i < TileData.Links.Num(); i++)
			{
				const NavMeshDebug::FOffMeshLink& SrcLink = TileData.Links[i];

				const FVector V0 = SrcLink.Left.ToVector() + OffsetLocation + CurrentData->NavMeshDrawOffset;
				const FVector V1 = SrcLink.Right.ToVector() + OffsetLocation + CurrentData->NavMeshDrawOffset;
				const FColor LinkColor = SrcLink.Color;

				FNavMeshRenderingHelpers_DEPRECATEDSUPPORT::CacheArc(CurrentData->NavLinkLines, V0, V1, 0.4f, 4, LinkColor, LinkLines_LineThickness);

				const FVector VOffset(0, 0, FVector::Dist(V0, V1) * 1.333f);
				FNavMeshRenderingHelpers_DEPRECATEDSUPPORT::CacheArrowHead(CurrentData->NavLinkLines, V1, V0+VOffset, 30.f, LinkColor, LinkLines_LineThickness);
				if (SrcLink.PackedFlags.Direction)
				{
					FNavMeshRenderingHelpers_DEPRECATEDSUPPORT::CacheArrowHead(CurrentData->NavLinkLines, V0, V1+VOffset, 30.f, LinkColor, LinkLines_LineThickness);
				}

				// if the connection as a whole is valid check if there are any of ends is invalid
				if (LinkColor != NavMeshRenderColor_OffMeshConnectionInvalid)
				{
					if (SrcLink.PackedFlags.Direction && (SrcLink.PackedFlags.ValidEnds & FRecastDebugGeometry::OMLE_Left) == 0)
					{
						// left end invalid - mark it
						FNavMeshRenderingHelpers_DEPRECATEDSUPPORT::DrawWireCylinder(CurrentData->NavLinkLines, V0, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), NavMeshRenderColor_OffMeshConnectionInvalid, SrcLink.PackedFlags.Radius, 30 /*NavMesh->AgentMaxStepHeight*/, 16, 0, DefaultEdges_LineThickness);
					}

					if ((SrcLink.PackedFlags.ValidEnds & FRecastDebugGeometry::OMLE_Right) == 0)
					{
						FNavMeshRenderingHelpers_DEPRECATEDSUPPORT::DrawWireCylinder(CurrentData->NavLinkLines, V1, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), NavMeshRenderColor_OffMeshConnectionInvalid, SrcLink.PackedFlags.Radius, 30 /*NavMesh->AgentMaxStepHeight*/, 16, 0, DefaultEdges_LineThickness);
					}
				}
			}
		}
	}
#endif
}

//----------------------------------------------------------------------//
// rendering
//----------------------------------------------------------------------//
class FPathDebugRenderSceneProxy : public FDebugRenderSceneProxy
{
public:
	FPathDebugRenderSceneProxy(const UPrimitiveComponent* InComponent, const FString &InViewFlagName)
		: FDebugRenderSceneProxy(InComponent)
	{
		DrawType = FDebugRenderSceneProxy::SolidAndWireMeshes;
		DrawAlpha = 90;
		ViewFlagName = InViewFlagName;
		ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*ViewFlagName));
		bWantsSelectionOutline = false;
	}


	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex);// IsShown(View);
		Result.bDynamicRelevance = true;
		// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
		Result.bSeparateTranslucencyRelevance = Result.bNormalTranslucencyRelevance = IsShown(View);
		return Result;
	}

};

FPrimitiveSceneProxy* UGameplayDebuggingComponent::CreateSceneProxy()
{
	FDebugRenderSceneCompositeProxy* CompositeProxy = nullptr;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	AGameplayDebuggingReplicator* Replicator = Cast<AGameplayDebuggingReplicator>(GetOwner());
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	if (!Replicator || !Replicator->IsDrawEnabled() || Replicator->IsPendingKill() || IsPendingKill())
	{
		return nullptr;
	}

#if WITH_RECAST	
	if (ShouldReplicateData(EAIDebugDrawDataView::NavMesh))
	{
		FNavMeshSceneProxyData NewNavmeshRenderData;
		NewNavmeshRenderData.Reset();
		NewNavmeshRenderData.bNeedsNewData = false;
		PrepareNavMeshData(&NewNavmeshRenderData);

		NavMeshBounds = NewNavmeshRenderData.Bounds.GetCenter().ContainsNaN() || NewNavmeshRenderData.Bounds.GetExtent().ContainsNaN() ? FBox(FVector(-HALF_WORLD_MAX1, -HALF_WORLD_MAX1, -HALF_WORLD_MAX1), FVector(HALF_WORLD_MAX1, HALF_WORLD_MAX1, HALF_WORLD_MAX1)) : NewNavmeshRenderData.Bounds;
		CompositeProxy = CompositeProxy ? CompositeProxy : (new FDebugRenderSceneCompositeProxy(this));
		CompositeProxy->AddChild(new FNavMeshSceneProxy(this, &NewNavmeshRenderData, true));
	}
#endif

#if USE_EQS_DEBUGGER
	if (ShouldReplicateData(EAIDebugDrawDataView::EQS) && IsClientEQSSceneProxyEnabled() && GetSelectedActor() != nullptr)
	{
		const int32 EQSIndex = EQSLocalData.Num() > 0 ? FMath::Clamp(CurrentEQSIndex, 0, EQSLocalData.Num() - 1) : INDEX_NONE;
		if (EQSLocalData.IsValidIndex(EQSIndex))
		{
			CompositeProxy = CompositeProxy ? CompositeProxy : (new FDebugRenderSceneCompositeProxy(this));
			auto& CurrentLocalData = EQSLocalData[EQSIndex];
			
			FString ViewFlagName = TEXT("Game");
#if WITH_EDITOR
			UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
			if (EEngine && EEngine->bIsSimulatingInEditor)
			{
				ViewFlagName = TEXT("DebugAI");
			}
#endif
			CompositeProxy->AddChild(new FEQSSceneProxy(this, ViewFlagName, CurrentLocalData.SolidSpheres, CurrentLocalData.Texts));
		}
	}
#endif // USE_EQS_DEBUGGER
	
	const bool bDrawFullData = Replicator->GetSelectedActorToDebug() == GetSelectedActor() && GetSelectedActor() != nullptr;
	if (bDrawFullData && ShouldReplicateData(EAIDebugDrawDataView::Basic))
	{
		CompositeProxy = CompositeProxy ? CompositeProxy : (new FDebugRenderSceneCompositeProxy(this));

		TArray<FDebugRenderSceneProxy::FMesh> Meshes;
		TArray<FDebugRenderSceneProxy::FDebugLine> Lines;
		for (FPathCorridorPolygons& CurrentPoly : PathCorridorPolygons)
		{

			if (CurrentPoly.Points.Num() > 2)
			{
				int32 LastIndex = 0;
				FVector FirstVertex = CurrentPoly.Points[0];
				FDebugRenderSceneProxy::FMesh TestMesh;
				TestMesh.Color = CurrentPoly.Color;// FColor::Green;
				for (int32 Index = 1; Index < CurrentPoly.Points.Num()-1; Index += 1)
				{
					TestMesh.Vertices.Add(FDynamicMeshVertex(FirstVertex));
					TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentPoly.Points[Index + 0]));
					TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentPoly.Points[Index + 1]));
					TestMesh.Indices.Add(LastIndex++);
					TestMesh.Indices.Add(LastIndex++);
					TestMesh.Indices.Add(LastIndex++);
				}
				Meshes.Add(TestMesh);
			}

			for (int32 VIdx = 0; VIdx < CurrentPoly.Points.Num(); VIdx++)
			{
				Lines.Add(FDebugRenderSceneProxy::FDebugLine(
					CurrentPoly.Points[VIdx],
					CurrentPoly.Points[(VIdx + 1) % CurrentPoly.Points.Num()],
					CurrentPoly.Color,
					2)
				);
			}
		}

		FString ViewFlagName = TEXT("Game");
#if WITH_EDITOR
		UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
		if (EEngine && EEngine->bIsSimulatingInEditor)
		{
			ViewFlagName = TEXT("DebugAI");
		}
#endif
		FPathDebugRenderSceneProxy *DebugSceneProxy = new FPathDebugRenderSceneProxy(this, ViewFlagName);
		DebugSceneProxy->Lines = Lines;
		DebugSceneProxy->Meshes = Meshes;
		CompositeProxy->AddChild(DebugSceneProxy);
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return CompositeProxy;
}

FBoxSphereBounds UGameplayDebuggingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox MyBounds;
	MyBounds.Init();

#if WITH_RECAST
	if (ShouldReplicateData(EAIDebugDrawDataView::NavMesh))
	{
		MyBounds = NavMeshBounds;
	}
#endif

#if USE_EQS_DEBUGGER
	if ((EQSRepData.Num() && ShouldReplicateData(EAIDebugDrawDataView::EQS)) || PathCorridorPolygons.Num())
	{
		MyBounds = FBox(FVector(-HALF_WORLD_MAX1, -HALF_WORLD_MAX1, -HALF_WORLD_MAX1), FVector(HALF_WORLD_MAX1, HALF_WORLD_MAX1, HALF_WORLD_MAX1));
	}
#endif // USE_EQS_DEBUGGER

	return MyBounds;
}

void UGameplayDebuggingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if WITH_EDITOR
	if (SceneProxy)
	{
		static_cast<FDebugRenderSceneCompositeProxy*>(SceneProxy)->RegisterDebugDrawDelgate();
	}
#endif
}

void UGameplayDebuggingComponent::DestroyRenderState_Concurrent()
{
#if WITH_EDITOR
	if (SceneProxy)
	{
		static_cast<FDebugRenderSceneCompositeProxy*>(SceneProxy)->UnregisterDebugDrawDelgate();
	}
#endif

	Super::DestroyRenderState_Concurrent();
}

void UGameplayDebuggingComponent::CollectPerceptionData()
{
#if USE_EQS_DEBUGGER
	if (!ShouldReplicateData(EAIDebugDrawDataView::Perception))
	{
		return;
	}

	APawn* MyPawn = Cast<APawn>(GetSelectedActor());
	if (MyPawn)
	{
		AAIController* BTAI = Cast<AAIController>(MyPawn->GetController());
		if (BTAI)
		{
			const UAIPerceptionComponent* PerceptionComponent = BTAI->GetAIPerceptionComponent();
			if (PerceptionComponent == nullptr)
			{
				PerceptionComponent = MyPawn->FindComponentByClass<UAIPerceptionComponent>();
			}
			if (PerceptionComponent)
			{
				FGameplayDebuggerCategory DummyCategory;
				PerceptionComponent->DescribeSelfToGameplayDebugger(&DummyCategory);

				TArray<FString> LoggedLines = DummyCategory.GetReplicatedLinesCopy();
				TArray<FGameplayDebuggerShape> LoggedShapes = DummyCategory.GetReplicatedShapesCopy();

				PerceptionShapeElements.Reset();
				for (int32 Idx = 0; Idx < LoggedShapes.Num(); Idx++)
				{
					const FGameplayDebuggerShape& ShapeItem = LoggedShapes[Idx];
					switch (LoggedShapes[Idx].Type)
					{
						case EGameplayDebuggerShape::Box:
							PerceptionShapeElements.Add(FGameplayDebuggerShapeElement::MakeBox(ShapeItem.ShapeData[0], ShapeItem.ShapeData[1], ShapeItem.Color, ShapeItem.Description));
							break;
						case EGameplayDebuggerShape::Capsule:
							PerceptionShapeElements.Add(FGameplayDebuggerShapeElement::MakeCapsule(ShapeItem.ShapeData[0], ShapeItem.ShapeData[1].X, ShapeItem.ShapeData[1].Z, ShapeItem.Color, ShapeItem.Description));
							break;
						case EGameplayDebuggerShape::Cone:
							PerceptionShapeElements.Add(FGameplayDebuggerShapeElement::MakeCone(ShapeItem.ShapeData[0], ShapeItem.ShapeData[1], ShapeItem.ShapeData[2].X, ShapeItem.Color, ShapeItem.Description));
							break;
						case EGameplayDebuggerShape::Cylinder:
							PerceptionShapeElements.Add(FGameplayDebuggerShapeElement::MakeCylinder(ShapeItem.ShapeData[0], ShapeItem.ShapeData[1].X, ShapeItem.ShapeData[1].Z, ShapeItem.Color, ShapeItem.Description));
							break;
						case EGameplayDebuggerShape::Point:
							PerceptionShapeElements.Add(FGameplayDebuggerShapeElement::MakePoint(ShapeItem.ShapeData[0], ShapeItem.ShapeData[1].X, ShapeItem.Color, ShapeItem.Description));
							break;
						case EGameplayDebuggerShape::Polygon:
							PerceptionShapeElements.Add(FGameplayDebuggerShapeElement::MakePolygon(ShapeItem.ShapeData, ShapeItem.Color, ShapeItem.Description));
							break;
						case EGameplayDebuggerShape::Segment:
							PerceptionShapeElements.Add(FGameplayDebuggerShapeElement::MakeSegment(ShapeItem.ShapeData[0], ShapeItem.ShapeData[1], ShapeItem.ShapeData[2].X, ShapeItem.Color, ShapeItem.Description));
							break;
						default:
							break;
					}
				}

				DistanceFromPlayer = DistanceFromSensor = -1;

				AGameplayDebuggingReplicator* Replicator = Cast<AGameplayDebuggingReplicator>(GetOwner());
				UGameplayDebuggingControllerComponent* GDC = Replicator ? Replicator->FindComponentByClass<UGameplayDebuggingControllerComponent>() : nullptr;
				APlayerController* MyPC = GDC && GDC->GetDebugCameraController().IsValid() ? GDC->GetDebugCameraController().Get() : Replicator->GetLocalPlayerOwner();

				if (MyPC && MyPC->GetPawn())
				{
					DistanceFromPlayer = (MyPawn->GetActorLocation() - MyPC->GetPawn()->GetActorLocation()).Size();
					DistanceFromSensor = SensingComponentLocation != FVector::ZeroVector ? (SensingComponentLocation - MyPC->GetPawn()->GetActorLocation()).Size() : -1;
				}

				PerceptionLegend = TEXT("\n");
				for (int32 Idx = 0; Idx < LoggedLines.Num(); Idx++)
				{
					PerceptionLegend += LoggedLines[Idx];
					PerceptionLegend += TEXT('\n');
				}
			}
		}
	}

#endif
}
