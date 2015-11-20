// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "GameplayTasksComponent.h"

//----------------------------------------------------------------------//
// UBTNode
//----------------------------------------------------------------------//

UBTNode::UBTNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ParentNode = NULL;
	TreeAsset = NULL;
	ExecutionIndex = 0;
	MemoryOffset = 0;
	TreeDepth = 0;
	bCreateNodeInstance = false;
	bIsInstanced = false;
	bIsInjected = false;

#if USE_BEHAVIORTREE_DEBUGGER
	NextExecutionNode = NULL;
#endif
}

UWorld* UBTNode::GetWorld() const
{
	// instanced nodes are created for behavior tree component owning that instance
	// template nodes are created for behavior tree manager, which is located directly in UWorld

	return GetOuter() == NULL ? NULL :
		IsInstanced() ? (Cast<UBehaviorTreeComponent>(GetOuter()))->GetWorld() :
		Cast<UWorld>(GetOuter()->GetOuter());
}

void UBTNode::InitializeNode(UBTCompositeNode* InParentNode, uint16 InExecutionIndex, uint16 InMemoryOffset, uint8 InTreeDepth)
{
	ParentNode = InParentNode;
	ExecutionIndex = InExecutionIndex;
	MemoryOffset = InMemoryOffset;
	TreeDepth = InTreeDepth;
}

void UBTNode::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	// empty in base 
}

void UBTNode::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	// empty in base 
}

void UBTNode::OnInstanceCreated(UBehaviorTreeComponent& OwnerComp)
{
	// empty in base class
}

void UBTNode::OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp)
{
	// empty in base class
}

void UBTNode::InitializeInSubtree(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, int32& NextInstancedIndex, EBTMemoryInit::Type InitType) const
{
	if (bCreateNodeInstance)
	{
		// composite nodes can't be instanced!
		check(IsA(UBTCompositeNode::StaticClass()) == false);

		UBTNode* NodeInstance = OwnerComp.NodeInstances.IsValidIndex(NextInstancedIndex) ? OwnerComp.NodeInstances[NextInstancedIndex] : NULL;
		if (NodeInstance == NULL)
		{
			NodeInstance = NewObject<UBTNode>(&OwnerComp, GetClass(), NAME_None, RF_NoFlags, (UObject*)(this));
			NodeInstance->InitializeNode(GetParentNode(), GetExecutionIndex(), GetMemoryOffset(), GetTreeDepth());
			NodeInstance->bIsInstanced = true;

			OwnerComp.NodeInstances.Add(NodeInstance);
		}
		check(NodeInstance);

		NodeInstance->SetOwner(OwnerComp.GetOwner());

		FBTInstancedNodeMemory* MyMemory = GetSpecialNodeMemory<FBTInstancedNodeMemory>(NodeMemory);
		MyMemory->NodeIdx = NextInstancedIndex;

		NodeInstance->OnInstanceCreated(OwnerComp);
		NextInstancedIndex++;
	}
	else
	{
		InitializeMemory(OwnerComp, NodeMemory, InitType);
	}
}

void UBTNode::CleanupInSubtree(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	if (!bCreateNodeInstance && !bIsInjected)
	{
		CleanupMemory(OwnerComp, NodeMemory, CleanupType);
	}
}

#if USE_BEHAVIORTREE_DEBUGGER
void UBTNode::InitializeExecutionOrder(UBTNode* NextNode)
{
	NextExecutionNode = NextNode;
}
#endif

void UBTNode::InitializeFromAsset(UBehaviorTree& Asset)
{
	TreeAsset = &Asset;
}

UBlackboardData* UBTNode::GetBlackboardAsset() const
{
	return TreeAsset ? TreeAsset->BlackboardAsset : NULL;
}

uint16 UBTNode::GetInstanceMemorySize() const
{
	return 0;
}

uint16 UBTNode::GetSpecialMemorySize() const
{
	return bCreateNodeInstance ? sizeof(FBTInstancedNodeMemory) : 0;
}

UBTNode* UBTNode::GetNodeInstance(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	FBTInstancedNodeMemory* MyMemory = GetSpecialNodeMemory<FBTInstancedNodeMemory>(NodeMemory);
	return MyMemory && OwnerComp.NodeInstances.IsValidIndex(MyMemory->NodeIdx) ?
		OwnerComp.NodeInstances[MyMemory->NodeIdx] : NULL;
}

UBTNode* UBTNode::GetNodeInstance(FBehaviorTreeSearchData& SearchData) const
{
	return GetNodeInstance(SearchData.OwnerComp, GetNodeMemory<uint8>(SearchData));
}

FString UBTNode::GetNodeName() const
{
	return NodeName.Len() ? NodeName : UBehaviorTreeTypes::GetShortTypeName(this);
}

FString UBTNode::GetRuntimeDescription(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity) const
{
	FString Description = NodeName.Len() ? FString::Printf(TEXT("%d. %s [%s]"), ExecutionIndex, *NodeName, *GetStaticDescription()) : GetStaticDescription();
	TArray<FString> RuntimeValues;

	const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(OwnerComp, NodeMemory) : this;
	if (NodeOb)
	{
		NodeOb->DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, RuntimeValues);
	}

	for (int32 ValueIndex = 0; ValueIndex < RuntimeValues.Num(); ValueIndex++)
	{
		Description += TEXT(", ");
		Description += RuntimeValues[ValueIndex];
	}

	return Description;
}

FString UBTNode::GetStaticDescription() const
{
	// short type name
	return UBehaviorTreeTypes::GetShortTypeName(this);
}

void UBTNode::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	// nothing stored in memory for base class
}

#if WITH_EDITOR

FName UBTNode::GetNodeIconName() const
{
	return NAME_None;
}

bool UBTNode::UsesBlueprint() const
{
	return false;
}

#endif

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
void UBTNode::InitializeMemory(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const
{
	if (OwnerComp)
	{
		InitializeMemory(*OwnerComp, NodeMemory, InitType);
	}
}
void UBTNode::CleanupMemory(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	if (OwnerComp)
	{
		CleanupMemory(*OwnerComp, NodeMemory, CleanupType);
	}
}
void UBTNode::DescribeRuntimeValues(const UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	if (OwnerComp)
	{
		DescribeRuntimeValues(*OwnerComp, NodeMemory, Verbosity, Values);
	}
}
void UBTNode::OnInstanceCreated(UBehaviorTreeComponent* OwnerComp)
{
	if (OwnerComp)
	{
		OnInstanceCreated(*OwnerComp);
	}
}
void UBTNode::OnInstanceDestroyed(UBehaviorTreeComponent* OwnerComp)
{
	if (OwnerComp)
	{
		OnInstanceDestroyed(*OwnerComp);
	}
}
void UBTNode::InitializeInSubtree(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, int32& NextInstancedIndex, EBTMemoryInit::Type InitType) const
{
	if (OwnerComp)
	{
		InitializeInSubtree(*OwnerComp, NodeMemory, NextInstancedIndex, InitType);
	}
}
void UBTNode::CleanupInSubtree(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	if (OwnerComp)
	{
		CleanupInSubtree(*OwnerComp, NodeMemory, CleanupType);
	}
}
UBTNode* UBTNode::GetNodeInstance(const UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const
{
	if (OwnerComp)
	{
		return GetNodeInstance(*OwnerComp, NodeMemory);
	}
	return nullptr;
}
FString UBTNode::GetRuntimeDescription(const UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity) const
{
	if (OwnerComp)
	{
		return GetRuntimeDescription(*OwnerComp, NodeMemory, Verbosity);
	}
	return TEXT("");
}


//----------------------------------------------------------------------//
// UBTNode IGameplayTaskOwnerInterface
//----------------------------------------------------------------------//
UGameplayTasksComponent* UBTNode::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	const UAITask* AsAITask = Cast<const UAITask>(&Task);
	if (AsAITask)
	{
		return AsAITask->GetAIController() ? AsAITask->GetAIController()->GetGameplayTasksComponent(Task) : nullptr;
	}

	return Task.GetGameplayTasksComponent();
}

void UBTNode::OnTaskInitialized(UGameplayTask& Task)
{
	// validate the task
	UAITask* AsAITask = Cast<UAITask>(&Task);
	if (AsAITask != nullptr && AsAITask->GetAIController() == nullptr)
	{
		// this means that the task has either been created without specifying 
		// UAITAsk::OwnerController's value (like via BP's Construct Object node)
		// or it has been created in C++ with inappropriate function
		UE_LOG(LogBehaviorTree, Error, TEXT("Missing AIController in AITask %s"), *AsAITask->GetName());
	}
}

UBehaviorTreeComponent* UBTNode::GetBTComponentForTask(UGameplayTask& Task) const
{
	UAITask* AsAITask = Cast<UAITask>(&Task);
	return AsAITask && AsAITask->GetAIController() ? Cast<UBehaviorTreeComponent>(AsAITask->GetAIController()->BrainComponent) : nullptr;
}

void UBTNode::OnTaskActivated(UGameplayTask& Task)
{
	ensure(Task.GetTaskOwner() == this);
}

void UBTNode::OnTaskDeactivated(UGameplayTask& Task)
{
	ensure(Task.GetTaskOwner() == this);
}

AActor* UBTNode::GetOwnerActor(const UGameplayTask* Task) const
{
	if (Task == nullptr)
	{
		if (IsInstanced())
		{
			const UBehaviorTreeComponent* BTComponent = Cast<const UBehaviorTreeComponent>(GetOuter());
			//not having BT component for an instanced BT node is invalid!
			check(BTComponent);
			return BTComponent->GetAIOwner();
		}
		else
		{
			UE_LOG(LogBehaviorTree, Warning, TEXT("%s: Unable to determine Owner Actor for a null GameplayTask"), *GetName());
			return nullptr;
		}
	}

	const UAITask* AsAITask = Cast<const UAITask>(Task);
	if (AsAITask)
	{
		return AsAITask->GetAIController();
	}

	const UGameplayTasksComponent* GTComponent = Task->GetGameplayTasksComponent();

	return GTComponent ? GTComponent->GetOwnerActor(Task) : nullptr;
}

AActor* UBTNode::GetAvatarActor(const UGameplayTask* Task) const
{
	if (Task == nullptr)
	{
		if (IsInstanced())
		{
			const UBehaviorTreeComponent* BTComponent = Cast<const UBehaviorTreeComponent>(GetOuter());
			//not having BT component for an instanced BT node is invalid!
			check(BTComponent);
			return BTComponent->GetAIOwner() ? BTComponent->GetAIOwner()->GetPawn() : nullptr;
		}
		else
		{
			UE_LOG(LogBehaviorTree, Warning, TEXT("%s: Unable to determine Avatar Actor for a null GameplayTask"), *GetName());
			return nullptr;
		}
	}

	const UAITask* AsAITask = Cast<const UAITask>(Task);
	if (AsAITask)
	{
		return AsAITask->GetAIController() ? AsAITask->GetAIController()->GetPawn() : nullptr;
	}

	const UGameplayTasksComponent* GTComponent = Task->GetGameplayTasksComponent();

	return GTComponent ? GTComponent->GetOwnerActor(Task) : nullptr;
}

uint8 UBTNode::GetDefaultPriority() const
{
	return uint8(EAITaskPriority::AutonomousAI);
}
