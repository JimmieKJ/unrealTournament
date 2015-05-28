// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimStateMachineTypes.h"
#include "AnimSequenceBase.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AnimBlueprint.h"

#include "AnimBlueprintGeneratedClass.generated.h"

struct FAnimNotifyEvent;
class  UAnimInstance;
class  UEdGraph;
class  UEdGraphNode;
class  UAnimGraphNode_StateResult;

// This structure represents debugging information for a single state machine
USTRUCT()
struct FStateMachineDebugData
{
	GENERATED_USTRUCT_BODY()

public:
	// Map from state nodes to their state entry in a state machine
	TMap<TWeakObjectPtr<UEdGraphNode>, int32> NodeToStateIndex;
	TMap<TWeakObjectPtr<UEdGraphNode>, int32> NodeToTransitionIndex;

	// The animation node that leads into this state machine (A3 only)
	TWeakObjectPtr<class UAnimGraphNode_StateMachineBase> MachineInstanceNode;

	// Index of this machine in the StateMachines array
	int32 MachineIndex;

public:
	ENGINE_API UEdGraphNode* FindNodeFromStateIndex(int32 StateIndex) const;
	ENGINE_API UEdGraphNode* FindNodeFromTransitionIndex(int32 TransitionIndex) const;
};

// This structure represents debugging information for a frame snapshot
USTRUCT()
struct FAnimationFrameSnapshot
{
	GENERATED_USTRUCT_BODY()

	FAnimationFrameSnapshot()
#if WITH_EDITORONLY_DATA
		: TimeStamp(0.0)
#endif
	{
	}
#if WITH_EDITORONLY_DATA
public:
	// The snapshot of data saved from the animation
	TArray<uint8> SerializedData;

	// The time stamp for when this snapshot was taken (relative to the life timer of the object being recorded)
	double TimeStamp;

public:
	void InitializeFromInstance(UAnimInstance* Instance);
	ENGINE_API void CopyToInstance(UAnimInstance* Instance);
#endif
};

// This structure represents animation-related debugging information for an entire AnimBlueprint
// (general debug information for the event graph, etc... is still contained in a FBlueprintDebugData structure)
USTRUCT()
struct ENGINE_API FAnimBlueprintDebugData
{
	GENERATED_USTRUCT_BODY()

	FAnimBlueprintDebugData()
#if WITH_EDITORONLY_DATA
		: SnapshotBuffer(NULL)
		, SnapshotIndex(INDEX_NONE)
#endif
	{
	}

#if WITH_EDITORONLY_DATA
public:
	// Map from state machine graphs to their corresponding debug data
	TMap<TWeakObjectPtr<UEdGraph>, FStateMachineDebugData> StateMachineDebugData;

	// Map from state graphs to their node
	TMap<TWeakObjectPtr<UEdGraph>, TWeakObjectPtr<class UAnimStateNode> > StateGraphToNodeMap;

	// Map from transition graphs to their node
	TMap<TWeakObjectPtr<UEdGraph>, TWeakObjectPtr<class UAnimStateTransitionNode> > TransitionGraphToNodeMap;

	// Map from custom transition blend graphs to their node
	TMap<TWeakObjectPtr<UEdGraph>, TWeakObjectPtr<class UAnimStateTransitionNode> > TransitionBlendGraphToNodeMap;

	// Map from animation node to their property index
	TMap<TWeakObjectPtr<class UAnimGraphNode_Base>, int32> NodePropertyToIndexMap;

	// Map from animation node GUID to property index
	TMap<FGuid, int32> NodeGuidToIndexMap;

	// History of snapshots of animation data
	TSimpleRingBuffer<FAnimationFrameSnapshot>* SnapshotBuffer;

	// Node visit structure
	struct FNodeVisit
	{
		int32 SourceID;
		int32 TargetID;
		float Weight;

		FNodeVisit(int32 InSourceID, int32 InTargetID, float InWeight)
			: SourceID(InSourceID)
			, TargetID(InTargetID)
			, Weight(InWeight)
		{
		}
	};

	// History of activated nodes
	TArray<FNodeVisit> UpdatedNodesThisFrame;

	// Index of snapshot
	int32 SnapshotIndex;
public:

	~FAnimBlueprintDebugData()
	{
		if (SnapshotBuffer != NULL)
		{
			delete SnapshotBuffer;
		}
		SnapshotBuffer = NULL;
	}



	bool IsReplayingSnapshot() const { return SnapshotIndex != INDEX_NONE; }
	void TakeSnapshot(UAnimInstance* Instance);
	float GetSnapshotLengthInSeconds();
	int32 GetSnapshotLengthInFrames();
	void SetSnapshotIndexByTime(UAnimInstance* Instance, double TargetTime);
	void SetSnapshotIndex(UAnimInstance* Instance, int32 NewIndex);
	void ResetSnapshotBuffer();

	void ResetNodeVisitSites();
	void RecordNodeVisit(int32 TargetNodeIndex, int32 SourceNodeIndex, float BlendWeight);
#endif
};

#if WITH_EDITORONLY_DATA
namespace EPropertySearchMode
{
	enum Type
	{
		OnlyThis,
		Hierarchy
	};
}
#endif

UCLASS()
class ENGINE_API UAnimBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

	// List of state machines present in this blueprint class
	UPROPERTY()
	TArray<FBakedAnimationStateMachine> BakedStateMachines;

	/** Target skeleton for this blueprint class */
	UPROPERTY()
	class USkeleton* TargetSkeleton;

	/** A list of anim notifies that state machines (or anything else) may reference */
	UPROPERTY()
	TArray<FAnimNotifyEvent> AnimNotifies;

	// The index of the root node in the animation tree
	UPROPERTY()
	int32 RootAnimNodeIndex;

	// The array of anim nodes; this is transient generated data (created during Link)
	UStructProperty* RootAnimNodeProperty;
	TArray<UStructProperty*> AnimNodeProperties;

#if WITH_EDITORONLY_DATA
	FAnimBlueprintDebugData AnimBlueprintDebugData;

	FAnimBlueprintDebugData& GetAnimBlueprintDebugData()
	{
		return AnimBlueprintDebugData;
	}

	template<typename StructType>
	const int32* GetNodePropertyIndexFromHierarchy(class UAnimGraphNode_Base* Node)
	{
		TArray<const UBlueprintGeneratedClass*> BlueprintHierarchy;
		GetGeneratedClassesHierarchy(this, BlueprintHierarchy);

		for (const UBlueprintGeneratedClass* Blueprint : BlueprintHierarchy)
		{
			if (const UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(Blueprint))
			{
				const int32* SearchIndex = AnimBlueprintClass->AnimBlueprintDebugData.NodePropertyToIndexMap.Find(Node);
				if (SearchIndex)
				{
					return SearchIndex;
				}
			}

		}
		return NULL;
	}

	template<typename StructType>
	UStructProperty* GetPropertyForNode(class UAnimGraphNode_Base* Node, EPropertySearchMode::Type SearchMode = EPropertySearchMode::OnlyThis)
	{
		const int32* pIndex = SearchMode == EPropertySearchMode::OnlyThis ? AnimBlueprintDebugData.NodePropertyToIndexMap.Find(Node) : GetNodePropertyIndexFromHierarchy<StructType>(Node);
		if (pIndex)
		{
			if (UStructProperty* AnimationProperty = AnimNodeProperties[AnimNodeProperties.Num() - 1 - *pIndex])
			{
				if (AnimationProperty->Struct->IsChildOf(StructType::StaticStruct()))
				{
					return AnimationProperty;
				}
			}
		}

		return NULL;
	}

	template<typename StructType>
	StructType* GetPropertyInstance(UObject* Object, class UAnimGraphNode_Base* Node, EPropertySearchMode::Type SearchMode = EPropertySearchMode::OnlyThis)
	{
		UStructProperty* AnimationProperty = GetPropertyForNode<StructType>(Node);
		if (AnimationProperty)
		{
			return AnimationProperty->ContainerPtrToValuePtr<StructType>((void*)Object);
		}

		return NULL;
	}

	template<typename StructType>
	StructType* GetPropertyInstance(UObject* Object, FGuid NodeGuid, EPropertySearchMode::Type SearchMode = EPropertySearchMode::OnlyThis)
	{
		const int32* pIndex = GetNodePropertyIndexFromGuid(NodeGuid, SearchMode);
		if (pIndex)
		{
			if (UStructProperty* AnimProperty = AnimNodeProperties[AnimNodeProperties.Num() - 1 - *pIndex])
			{
				if (AnimProperty->Struct->IsChildOf(StructType::StaticStruct()))
				{
					return AnimProperty->ContainerPtrToValuePtr<StructType>((void*)Object);
				}
			}
		}

		return NULL;
	}

	template<typename StructType>
	StructType& GetPropertyInstanceChecked(UObject* Object, class UAnimGraphNode_Base* Node, EPropertySearchMode::Type SearchMode = EPropertySearchMode::OnlyThis)
	{
		const int32 Index = AnimBlueprintDebugData.NodePropertyToIndexMap.FindChecked(Node);
		UStructProperty* AnimationProperty = AnimNodeProperties[AnimNodeProperties.Num() - 1 - Index];
		check(AnimationProperty);
		check(AnimationProperty->Struct->IsChildOf(StructType::StaticStruct()));
		return AnimationProperty->ContainerPtrToValuePtr<StructType>((void*)Object);
	}

	const int32* GetNodePropertyIndexFromGuid(FGuid Guid, EPropertySearchMode::Type SearchMode = EPropertySearchMode::OnlyThis);

#endif

	// UStruct interface
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	// End of UStruct interface

	// UClass interface
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	// End of UClass interface

#if WITH_EDITOR
public:
	class USkeleton* GetTargetSkeleton() const { return TargetSkeleton; }
#endif
};

template<typename NodeType>
NodeType* GetNodeFromPropertyIndex(UAnimInstance* AnimInstance, const UAnimBlueprintGeneratedClass* AnimBlueprintClass, int32 PropertyIndex)
{
	if (PropertyIndex != INDEX_NONE)
	{
		UStructProperty* NodeProperty = AnimBlueprintClass->AnimNodeProperties[AnimBlueprintClass->AnimNodeProperties.Num() - 1 - PropertyIndex]; //@TODO: Crazysauce
		check(NodeProperty->Struct == NodeType::StaticStruct());
		return NodeProperty->ContainerPtrToValuePtr<NodeType>(AnimInstance);
	}

	return NULL;
}