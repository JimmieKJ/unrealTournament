// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Blackboard/BlackboardKeyAllTypes.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTDecorator.h"

//----------------------------------------------------------------------//
// FBehaviorTreeInstance
//----------------------------------------------------------------------//
void FBehaviorTreeInstance::Initialize(UBehaviorTreeComponent& OwnerComp, UBTCompositeNode& Node, int32& InstancedIndex, EBTMemoryInit::Type InitType)
{
	for (int32 ServiceIndex = 0; ServiceIndex < Node.Services.Num(); ServiceIndex++)
	{
		Node.Services[ServiceIndex]->InitializeInSubtree(OwnerComp, Node.Services[ServiceIndex]->GetNodeMemory<uint8>(*this), InstancedIndex, InitType);
	}

	uint8* NodeMemory = Node.GetNodeMemory<uint8>(*this);
	Node.InitializeInSubtree(OwnerComp, NodeMemory, InstancedIndex, InitType);

	UBTCompositeNode* InstancedComposite = Cast<UBTCompositeNode>(Node.GetNodeInstance(OwnerComp, NodeMemory));
	if (InstancedComposite)
	{
		InstancedComposite->InitializeComposite(Node.GetLastExecutionIndex());
	}

	for (int32 ChildIndex = 0; ChildIndex < Node.Children.Num(); ChildIndex++)
	{
		FBTCompositeChild& ChildInfo = Node.Children[ChildIndex];

		for (int32 DecoratorIndex = 0; DecoratorIndex < ChildInfo.Decorators.Num(); DecoratorIndex++)
		{
			UBTDecorator* DecoratorOb = ChildInfo.Decorators[DecoratorIndex];
			uint8* DecoratorMemory = DecoratorOb->GetNodeMemory<uint8>(*this);
			DecoratorOb->InitializeInSubtree(OwnerComp, DecoratorMemory, InstancedIndex, InitType);

			UBTDecorator* InstancedDecoratorOb = Cast<UBTDecorator>(DecoratorOb->GetNodeInstance(OwnerComp, DecoratorMemory));
			if (InstancedDecoratorOb)
			{
				InstancedDecoratorOb->InitializeDecorator(DecoratorOb->GetChildIndex());
			}
		}

		if (ChildInfo.ChildComposite)
		{
			Initialize(OwnerComp, *(ChildInfo.ChildComposite), InstancedIndex, InitType);
		}
		else if (ChildInfo.ChildTask)
		{
			ChildInfo.ChildTask->InitializeInSubtree(OwnerComp, ChildInfo.ChildTask->GetNodeMemory<uint8>(*this), InstancedIndex, InitType);
		}
	}
}

void FBehaviorTreeInstance::InjectNodes(UBehaviorTreeComponent& OwnerComp, UBTCompositeNode& Node, int32& InstancedIndex)
{
	for (int32 ChildIndex = 0; ChildIndex < Node.Children.Num(); ChildIndex++)
	{
		FBTCompositeChild& ChildInfo = Node.Children[ChildIndex];
		if (ChildInfo.ChildComposite)
		{
			InjectNodes(OwnerComp, *(ChildInfo.ChildComposite), InstancedIndex);
		}
		else
		{
			UBTTask_RunBehavior* InjectingTask = Cast<UBTTask_RunBehavior>(ChildInfo.ChildTask);
			if (InjectingTask)
			{
				uint8* NodeMemory = InjectingTask->GetNodeMemory<uint8>(*this);
				InjectingTask->InjectNodes(OwnerComp, NodeMemory, InstancedIndex);
			}
		}
	}
}

void FBehaviorTreeInstance::Cleanup(UBehaviorTreeComponent& OwnerComp, EBTMemoryClear::Type CleanupType)
{
	FBehaviorTreeInstanceId& Info = OwnerComp.KnownInstances[InstanceIdIndex];
	if (Info.FirstNodeInstance >= 0)
	{
		const int32 MaxAllowedIdx = OwnerComp.NodeInstances.Num();
		const int32 LastNodeIdx = OwnerComp.KnownInstances.IsValidIndex(InstanceIdIndex + 1) ?
			FMath::Min(OwnerComp.KnownInstances[InstanceIdIndex + 1].FirstNodeInstance, MaxAllowedIdx) :
			MaxAllowedIdx;

		for (int32 Idx = Info.FirstNodeInstance; Idx < LastNodeIdx; Idx++)
		{
			OwnerComp.NodeInstances[Idx]->OnInstanceDestroyed(OwnerComp);
		}
	}

	CleanupNodes(OwnerComp, *RootNode, CleanupType);
	Info.InstanceMemory = InstanceMemory;
}

void FBehaviorTreeInstance::CleanupNodes(UBehaviorTreeComponent& OwnerComp, UBTCompositeNode& Node, EBTMemoryClear::Type CleanupType)
{
	for (int32 ServiceIndex = 0; ServiceIndex < Node.Services.Num(); ServiceIndex++)
	{
		Node.Services[ServiceIndex]->CleanupInSubtree(OwnerComp, Node.Services[ServiceIndex]->GetNodeMemory<uint8>(*this), CleanupType);
	}

	Node.CleanupInSubtree(OwnerComp, Node.GetNodeMemory<uint8>(*this), CleanupType);

	for (int32 ChildIndex = 0; ChildIndex < Node.Children.Num(); ChildIndex++)
	{
		FBTCompositeChild& ChildInfo = Node.Children[ChildIndex];

		for (int32 DecoratorIndex = 0; DecoratorIndex < ChildInfo.Decorators.Num(); DecoratorIndex++)
		{
			ChildInfo.Decorators[DecoratorIndex]->CleanupInSubtree(OwnerComp, ChildInfo.Decorators[DecoratorIndex]->GetNodeMemory<uint8>(*this), CleanupType);
		}

		if (ChildInfo.ChildComposite)
		{
			CleanupNodes(OwnerComp, *(ChildInfo.ChildComposite), CleanupType);
		}
		else if (ChildInfo.ChildTask)
		{
			ChildInfo.ChildTask->CleanupInSubtree(OwnerComp, ChildInfo.ChildTask->GetNodeMemory<uint8>(*this), CleanupType);
		}
	}
}


//----------------------------------------------------------------------//
// FBTNodeIndex
//----------------------------------------------------------------------//

bool FBTNodeIndex::TakesPriorityOver(const FBTNodeIndex& Other) const
{
	// instance closer to root is more important
	if (InstanceIndex != Other.InstanceIndex)
	{
		return InstanceIndex < Other.InstanceIndex;
	}

	// higher priority is more important
	return ExecutionIndex < Other.ExecutionIndex;
}

//----------------------------------------------------------------------//
// FBehaviorTreeSearchData
//----------------------------------------------------------------------//

int32 FBehaviorTreeSearchData::NextSearchId = 1;

void FBehaviorTreeSearchData::AddUniqueUpdate(const FBehaviorTreeSearchUpdate& UpdateInfo)
{
	UE_VLOG(OwnerComp.GetOwner(), LogBehaviorTree, Verbose, TEXT("Search node update[%s]: %s"),
		*UBehaviorTreeTypes::DescribeNodeUpdateMode(UpdateInfo.Mode),
		*UBehaviorTreeTypes::DescribeNodeHelper(UpdateInfo.AuxNode ? (UBTNode*)UpdateInfo.AuxNode : (UBTNode*)UpdateInfo.TaskNode));

	bool bSkipAdding = false;
	for (int32 UpdateIndex = 0; UpdateIndex < PendingUpdates.Num(); UpdateIndex++)
	{
		const FBehaviorTreeSearchUpdate& Info = PendingUpdates[UpdateIndex];
		if (Info.AuxNode == UpdateInfo.AuxNode && Info.TaskNode == UpdateInfo.TaskNode)
		{
			// duplicate, skip
			if (Info.Mode == UpdateInfo.Mode)
			{
				bSkipAdding = true;
				break;
			}

			// don't add pairs add-remove
			bSkipAdding = (Info.Mode == EBTNodeUpdateMode::Remove) || (UpdateInfo.Mode == EBTNodeUpdateMode::Remove);

			PendingUpdates.RemoveAt(UpdateIndex, 1, false);
		}
	}

	if (!bSkipAdding)
	{
		const int32 Idx = PendingUpdates.Add(UpdateInfo);
		PendingUpdates[Idx].bPostUpdate = (UpdateInfo.Mode == EBTNodeUpdateMode::Add) && (Cast<UBTService>(UpdateInfo.AuxNode) != NULL);
	}
}

void FBehaviorTreeSearchData::AssignSearchId()
{
	SearchId = NextSearchId;
	NextSearchId++;
}

//----------------------------------------------------------------------//
// FBlackboardKeySelector
//----------------------------------------------------------------------//
void FBlackboardKeySelector::CacheSelectedKey(UBlackboardData* BlackboardAsset)
{
	if (BlackboardAsset && !(bNoneIsAllowedValue && SelectedKeyName == NAME_None))
	{
		if (SelectedKeyName == NAME_None)
		{
			InitSelectedKey(BlackboardAsset);
		}

		SelectedKeyID = BlackboardAsset->GetKeyID(SelectedKeyName);
		SelectedKeyType = BlackboardAsset->GetKeyType(SelectedKeyID);
	}
}

void FBlackboardKeySelector::InitSelectedKey(UBlackboardData* BlackboardAsset)
{
	check(BlackboardAsset);
	for (UBlackboardData* It = BlackboardAsset; It; It = It->Parent)
	{
		for (int32 KeyIndex = 0; KeyIndex < It->Keys.Num(); KeyIndex++)
		{
			const FBlackboardEntry& EntryInfo = It->Keys[KeyIndex];
			if (EntryInfo.KeyType)
			{
				bool bFilterPassed = false;
				if (AllowedTypes.Num())
				{
					for (int32 FilterIndex = 0; FilterIndex < AllowedTypes.Num(); FilterIndex++)
					{
						if (EntryInfo.KeyType->IsAllowedByFilter(AllowedTypes[FilterIndex]))
						{
							bFilterPassed = true;
							break;
						}
					}
				}
				else
				{
					bFilterPassed = true;
				}

				SelectedKeyName = EntryInfo.EntryName;
				break;
			}
		}
	}
}

void FBlackboardKeySelector::AddObjectFilter(UObject* Owner, TSubclassOf<UObject> AllowedClass)
{
	AddObjectFilter(Owner, TEXT("BlackboardKeyType"), AllowedClass);
}

void FBlackboardKeySelector::AddClassFilter(UObject* Owner, TSubclassOf<UClass> AllowedClass)
{
	AddClassFilter(Owner, TEXT("BlackboardKeyType"), AllowedClass);
}

void FBlackboardKeySelector::AddEnumFilter(UObject* Owner, UEnum* AllowedEnum)
{
	AddEnumFilter(Owner, TEXT("BlackboardKeyType"), AllowedEnum);
}

void FBlackboardKeySelector::AddNativeEnumFilter(UObject* Owner, const FString& AllowedEnumName)
{
	AddNativeEnumFilter(Owner, TEXT("BlackboardKeyType"), AllowedEnumName);
}

void FBlackboardKeySelector::AddIntFilter(UObject* Owner)
{
	AddIntFilter(Owner, TEXT("BlackboardKeyType"));
}

void FBlackboardKeySelector::AddFloatFilter(UObject* Owner)
{
	AddFloatFilter(Owner, TEXT("BlackboardKeyType"));
}

void FBlackboardKeySelector::AddBoolFilter(UObject* Owner)
{
	AddBoolFilter(Owner, TEXT("BlackboardKeyType"));
}

void FBlackboardKeySelector::AddVectorFilter(UObject* Owner)
{
	AddVectorFilter(Owner, TEXT("BlackboardKeyType"));
}

void FBlackboardKeySelector::AddRotatorFilter(UObject* Owner)
{
	AddRotatorFilter(Owner, TEXT("BlackboardKeyType"));
}

void FBlackboardKeySelector::AddStringFilter(UObject* Owner)
{
	AddStringFilter(Owner, TEXT("BlackboardKeyType"));
}

void FBlackboardKeySelector::AddNameFilter(UObject* Owner)
{
	AddNameFilter(Owner, TEXT("BlackboardKeyType"));
}

void FBlackboardKeySelector::AddObjectFilter(UObject* Owner, FName PropertyName, TSubclassOf<UObject> AllowedClass)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Object");
	UBlackboardKeyType_Object* FilterOb = Owner->CreateDefaultSubobject<UBlackboardKeyType_Object>(*FilterName);
	FilterOb->BaseClass = AllowedClass;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddClassFilter(UObject* Owner, FName PropertyName, TSubclassOf<UClass> AllowedClass)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Class");
	UBlackboardKeyType_Class* FilterOb = Owner->CreateDefaultSubobject<UBlackboardKeyType_Class>(*FilterName);
	FilterOb->BaseClass = AllowedClass;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddEnumFilter(UObject* Owner, FName PropertyName, UEnum* AllowedEnum)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Enum");
	UBlackboardKeyType_Enum* FilterOb = Owner->CreateDefaultSubobject<UBlackboardKeyType_Enum>(*FilterName);
	FilterOb->EnumType = AllowedEnum;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddNativeEnumFilter(UObject* Owner, FName PropertyName, const FString& AllowedEnumName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_NativeEnum");
	UBlackboardKeyType_NativeEnum* FilterOb = Owner->CreateDefaultSubobject<UBlackboardKeyType_NativeEnum>(*FilterName);
	FilterOb->EnumName = AllowedEnumName;
	AllowedTypes.Add(FilterOb);
}

void FBlackboardKeySelector::AddIntFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Int");
	AllowedTypes.Add(Owner->CreateDefaultSubobject<UBlackboardKeyType_Int>(*FilterName));
}

void FBlackboardKeySelector::AddFloatFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Float");
	AllowedTypes.Add(Owner->CreateDefaultSubobject<UBlackboardKeyType_Float>(*FilterName));
}

void FBlackboardKeySelector::AddBoolFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Bool");
	AllowedTypes.Add(Owner->CreateDefaultSubobject<UBlackboardKeyType_Bool>(*FilterName));
}

void FBlackboardKeySelector::AddVectorFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Vector");
	AllowedTypes.Add(Owner->CreateDefaultSubobject<UBlackboardKeyType_Vector>(*FilterName));
}

void FBlackboardKeySelector::AddRotatorFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Rotator");
	AllowedTypes.Add(Owner->CreateDefaultSubobject<UBlackboardKeyType_Rotator>(*FilterName));
}

void FBlackboardKeySelector::AddStringFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_String");
	AllowedTypes.Add(Owner->CreateDefaultSubobject<UBlackboardKeyType_String>(*FilterName));
}

void FBlackboardKeySelector::AddNameFilter(UObject* Owner, FName PropertyName)
{
	const FString FilterName = PropertyName.ToString() + TEXT("_Name");
	AllowedTypes.Add(Owner->CreateDefaultSubobject<UBlackboardKeyType_Name>(*FilterName));
}

//----------------------------------------------------------------------//
// UBehaviorTreeTypes
//----------------------------------------------------------------------//
UBehaviorTreeTypes::UBehaviorTreeTypes(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FString UBehaviorTreeTypes::DescribeNodeResult(EBTNodeResult::Type NodeResult)
{
	static FString ResultDesc[] = { TEXT("Succeeded"), TEXT("Failed"), TEXT("Aborted"), TEXT("InProgress") };
	return (NodeResult < ARRAY_COUNT(ResultDesc)) ? ResultDesc[NodeResult] : FString();
}

FString UBehaviorTreeTypes::DescribeFlowAbortMode(EBTFlowAbortMode::Type AbortMode)
{
	static FString AbortModeDesc[] = { TEXT("None"), TEXT("Lower Priority"), TEXT("Self"), TEXT("Both") };
	return (AbortMode < ARRAY_COUNT(AbortModeDesc)) ? AbortModeDesc[AbortMode] : FString();
}

FString UBehaviorTreeTypes::DescribeActiveNode(EBTActiveNode::Type ActiveNodeType)
{
	static FString ActiveDesc[] = { TEXT("Composite"), TEXT("ActiveTask"), TEXT("AbortingTask"), TEXT("InactiveTask") };
	return (ActiveNodeType < ARRAY_COUNT(ActiveDesc)) ? ActiveDesc[ActiveNodeType] : FString();
}

FString UBehaviorTreeTypes::DescribeTaskStatus(EBTTaskStatus::Type TaskStatus)
{
	static FString TaskStatusDesc[] = { TEXT("Active"), TEXT("Aborting"), TEXT("Inactive") };
	return (TaskStatus < ARRAY_COUNT(TaskStatusDesc)) ? TaskStatusDesc[TaskStatus] : FString();
}

FString UBehaviorTreeTypes::DescribeNodeUpdateMode(EBTNodeUpdateMode::Type UpdateMode)
{
	static FString UpdateModeDesc[] = { TEXT("Add"), TEXT("Remove") };
	return (UpdateMode < ARRAY_COUNT(UpdateModeDesc)) ? UpdateModeDesc[UpdateMode] : FString();
}

FString UBehaviorTreeTypes::DescribeNodeHelper(const UBTNode* Node)
{
	return Node ? FString::Printf(TEXT("%s[%d]"), *Node->GetNodeName(), Node->GetExecutionIndex()) : FString();
}

FString UBehaviorTreeTypes::GetShortTypeName(const UObject* Ob)
{
	if (Ob->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return Ob->GetClass()->GetName().LeftChop(2);
	}

	FString TypeDesc = Ob->GetClass()->GetName();
	const int32 ShortNameIdx = TypeDesc.Find(TEXT("_"));
	if (ShortNameIdx != INDEX_NONE)
	{
		TypeDesc = TypeDesc.Mid(ShortNameIdx + 1);
	}

	return TypeDesc;
}
