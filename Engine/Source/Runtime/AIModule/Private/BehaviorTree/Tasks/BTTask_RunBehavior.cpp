// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/BehaviorTreeManager.h"

// Each injected decorator must be instanced before using in parent tree, because it's not part of it
// This requires special memory handling for decorator which were not meant to be instanced
// and need their memory block for storage.
//
// Memory allocated for BTTask_Behavior consist of header and N pairs for each injected decorator:
//
// ---- task memory start, address stored in Task.MemoryOffset
// - FBTInstancedNodeMemory: task's header with index of first instanced decorator
// ---- injected decorators memory start
// - FBTInstancedNodeMemory: index of injected decorator 1 (special node memory)
// - memory_required_by_decorator: memory block requested by decorator1, address stored in Decorator1.MemoryOffset, 
// - (remaining pairs)
//
//
// Each decorator must provide required memory size with instance index header.
// Decorator memory will be accessible in continuous block, MemoryOffset points at data inside task's memory block
// Decorators which should be instanced will access their own object through index in special node memory headers

UBTTask_RunBehavior::UBTTask_RunBehavior(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Run Behavior";
}

EBTNodeResult::Type UBTTask_RunBehavior::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const bool bPushed = BehaviorAsset != nullptr && OwnerComp.PushInstance(*BehaviorAsset);
	return bPushed ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

FString UBTTask_RunBehavior::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *GetNameSafe(BehaviorAsset));
}

uint16 UBTTask_RunBehavior::GetInstanceMemorySize() const
{
	// memory required by root level decorators is provided by this task
	// it's not possible to have dynamic subtrees supporting root level decorators
	// (unnecessary complication of accessing node's memory)

	int32 MemorySize = 0;
	if (BehaviorAsset)
	{
		// all decorators need to return memory requirements as instanced nodes (include header sizes)

		TArray<uint16> MemoryOffsets;
		const bool bForceInstancing = true;
		UBehaviorTreeManager::InitializeMemoryHelper(BehaviorAsset->RootDecorators, MemoryOffsets, MemorySize, bForceInstancing);
	}

	// memory block header: index of first node instance in owner's array
	MemorySize += sizeof(FBTInstancedNodeMemory);

	return MemorySize;
}

void UBTTask_RunBehavior::InjectNodes(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, int32& InstancedIndex) const
{
	if (BehaviorAsset == NULL || BehaviorAsset->RootDecorators.Num() == 0)
	{
		return;
	}

	const int32 NumInjectedDecorators = BehaviorAsset->RootDecorators.Num();
	FBTInstancedNodeMemory* NodeMemoryHeader = (FBTInstancedNodeMemory*)NodeMemory;
	int32 FirstNodeIdx = NodeMemoryHeader->NodeIdx;

	const uint16 HeaderSize = sizeof(FBTInstancedNodeMemory);
	const uint16 InjectedMemoryOffset = GetMemoryOffset() + HeaderSize;
	uint8* InjectedMemoryBase = NodeMemory + HeaderSize;

	// initialize on first access
	if (!OwnerComp.NodeInstances.IsValidIndex(InstancedIndex))
	{
		TArray<uint16> MemoryOffsets;
		int32 MemorySize = 0;

		const bool bForceInstancing = true;
		UBehaviorTreeManager::InitializeMemoryHelper(BehaviorAsset->RootDecorators, MemoryOffsets, MemorySize, bForceInstancing);

		// newly created nodes = full init
		EBTMemoryInit::Type InitType = EBTMemoryInit::Initialize;

		NodeMemoryHeader->NodeIdx = InstancedIndex;
		FirstNodeIdx = InstancedIndex;

		// create nodes
		for (int32 Idx = 0; Idx < NumInjectedDecorators; Idx++)
		{
			UBTDecorator* DecoratorOb = BehaviorAsset->RootDecorators[Idx];

			const bool bUsesInstancing = DecoratorOb->HasInstance();
			if (bUsesInstancing)
			{
				DecoratorOb->InitializeInSubtree(OwnerComp, InjectedMemoryBase + MemoryOffsets[Idx], InstancedIndex, InitType);
			}
			else
			{
				DecoratorOb->ForceInstancing(true);
				DecoratorOb->InitializeInSubtree(OwnerComp, InjectedMemoryBase + MemoryOffsets[Idx], InstancedIndex, InitType);
				DecoratorOb->ForceInstancing(false);

				UBTDecorator* InstancedOb = Cast<UBTDecorator>(OwnerComp.NodeInstances.Last());
				InstancedOb->InitializeMemory(OwnerComp, InjectedMemoryBase + MemoryOffsets[Idx], InitType);
			}
		}

		// setup their properties
		uint16 NewExecutionIdx = GetExecutionIndex() - GetInjectedNodesCount();
		for (int32 Idx = 0; Idx < NumInjectedDecorators; Idx++)
		{
			UBTDecorator* InstancedOb = Cast<UBTDecorator>(OwnerComp.NodeInstances[FirstNodeIdx + Idx]);
			InstancedOb->InitializeNode(GetParentNode(), NewExecutionIdx, InjectedMemoryOffset + MemoryOffsets[Idx], GetTreeDepth() - 1);
			InstancedOb->MarkInjectedNode();

			NewExecutionIdx++;
		}
	}
	else
	{
		// restoring existing nodes = partial init
		EBTMemoryInit::Type InitType = EBTMemoryInit::RestoreSubtree;

		// initialize memory for injected nodes
		for (int32 Idx = 0; Idx < NumInjectedDecorators; Idx++)
		{
			UBTDecorator* DecoratorOb = BehaviorAsset->RootDecorators[Idx];
			UBTDecorator* InstancedOb = Cast<UBTDecorator>(OwnerComp.NodeInstances[FirstNodeIdx + Idx]);

			const bool bUsesInstancing = DecoratorOb->HasInstance();
			if (bUsesInstancing)
			{
				InstancedOb->OnInstanceCreated(OwnerComp);
			}
			else
			{
				uint8* InjectedNodeMemory = NodeMemory + (InstancedOb->GetMemoryOffset() - GetMemoryOffset());
				InstancedOb->InitializeMemory(OwnerComp, InjectedNodeMemory, InitType);
			}
		}

		InstancedIndex += NumInjectedDecorators;
	}

	// inject nodes
	if (GetParentNode())
	{
		const int32 ChildIdx = GetParentNode()->GetChildIndex(*this);
		if (ChildIdx != INDEX_NONE)
		{
			FBTCompositeChild& LinkData = GetParentNode()->Children[ChildIdx];

			// check if link already has injected decorators
			bool bAlreadyInjected = false;
			for (int32 Idx = 0; Idx < LinkData.Decorators.Num(); Idx++)
			{
				if (LinkData.Decorators[Idx] && LinkData.Decorators[Idx]->IsInjected())
				{
					bAlreadyInjected = true;
					break;
				}
			}

			// add decorators to link
			const int32 NumOriginalDecorators = LinkData.Decorators.Num();
			for (int32 Idx = 0; Idx < NumInjectedDecorators; Idx++)
			{
				UBTDecorator* InstancedOb = Cast<UBTDecorator>(OwnerComp.NodeInstances[FirstNodeIdx + Idx]);
				InstancedOb->InitializeFromAsset(*BehaviorAsset);
				InstancedOb->InitializeDecorator(ChildIdx);

				if (!bAlreadyInjected)
				{
					LinkData.Decorators.Add(InstancedOb);
				}
			}

			// update composite logic operators
			if (!bAlreadyInjected && (LinkData.DecoratorOps.Num() || BehaviorAsset->RootDecoratorOps.Num()))
			{
				const int32 NumOriginalOps = LinkData.DecoratorOps.Num();
				if (NumOriginalDecorators > 0)
				{
					// and operator for two groups of composites: original and new one
					FBTDecoratorLogic MasterAndOp(EBTDecoratorLogic::And, LinkData.DecoratorOps.Num() ? 2 : (NumOriginalDecorators + 1));
					LinkData.DecoratorOps.Insert(MasterAndOp, 0);

					if (NumOriginalOps == 0)
					{
						// add Test operations, original link didn't have composite operators
						for (int32 Idx = 0; Idx < NumOriginalDecorators; Idx++)
						{
							FBTDecoratorLogic TestOp(EBTDecoratorLogic::Test, Idx);
							LinkData.DecoratorOps.Add(TestOp);
						}
					}
				}

				// add injected operators
				if (BehaviorAsset->RootDecoratorOps.Num() == 0)
				{
					FBTDecoratorLogic InjectedAndOp(EBTDecoratorLogic::And, NumInjectedDecorators);
					LinkData.DecoratorOps.Add(InjectedAndOp);

					for (int32 Idx = 0; Idx < NumInjectedDecorators; Idx++)
					{
						FBTDecoratorLogic TestOp(EBTDecoratorLogic::Test, NumOriginalDecorators + Idx);
						LinkData.DecoratorOps.Add(TestOp);
					}
				}
				else
				{
					for (int32 Idx = 0; Idx < BehaviorAsset->RootDecoratorOps.Num(); Idx++)
					{
						FBTDecoratorLogic InjectedOp = BehaviorAsset->RootDecoratorOps[Idx];
						if (InjectedOp.Operation == EBTDecoratorLogic::Test)
						{
							InjectedOp.Number += NumOriginalDecorators;
						}

						LinkData.DecoratorOps.Add(InjectedOp);
					}
				}
			}

#if USE_BEHAVIORTREE_DEBUGGER
			if (!bAlreadyInjected)
			{
				// insert to NextExecutionNode list for debugger
				UBTNode* NodeIt = GetParentNode();
				while (NodeIt && NodeIt->GetNextNode() != this)
				{
					NodeIt = NodeIt->GetNextNode();
				}

				if (NodeIt)
				{
					NodeIt->InitializeExecutionOrder(OwnerComp.NodeInstances[FirstNodeIdx]);
					NodeIt = NodeIt->GetNextNode();

					for (int32 Idx = 1; Idx < NumInjectedDecorators; Idx++)
					{
						NodeIt->InitializeExecutionOrder(OwnerComp.NodeInstances[FirstNodeIdx + Idx]);
						NodeIt = NodeIt->GetNextNode();
					}

					NodeIt->InitializeExecutionOrder((UBTNode*)this);
				}
			}
#endif
		}
	}
}

void UBTTask_RunBehavior::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const
{
	Super::CleanupMemory(OwnerComp, NodeMemory, CleanupType);

	if (GetParentNode() && BehaviorAsset)
	{
		FBTInstancedNodeMemory* NodeMemoryHeader = (FBTInstancedNodeMemory*)NodeMemory;

		const int32 NumInjectedDecorators = BehaviorAsset->RootDecorators.Num();
		for (int32 Idx = 0; Idx < NumInjectedDecorators; Idx++)
		{
			const int32 InstancedNodeIdx = NodeMemoryHeader->NodeIdx + Idx;
			UBTDecorator* InstancedOb = OwnerComp.NodeInstances.IsValidIndex(InstancedNodeIdx) ?
				Cast<UBTDecorator>(OwnerComp.NodeInstances[InstancedNodeIdx]) : NULL;

			if (InstancedOb)
			{
				uint8* InjectedNodeMemory = NodeMemory + (InstancedOb->GetMemoryOffset() - GetMemoryOffset());
				InstancedOb->CleanupMemory(OwnerComp, InjectedNodeMemory, CleanupType);
			}
		}
	}
}

#if WITH_EDITOR

FName UBTTask_RunBehavior::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.RunBehavior.Icon");
}

#endif	// WITH_EDITOR
