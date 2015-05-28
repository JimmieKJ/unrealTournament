// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTreeComponent.h"
#include "BehaviorTreeTypes.h"
#include "BTNode.generated.h"

class AAIController;
class UWorld;
class UBehaviorTree;
class UBehaviorTreeComponent;
class UBTCompositeNode;
class UBlackboardData;
struct FBehaviorTreeSearchData;

struct FBTInstancedNodeMemory
{
	int32 NodeIdx;
};

UCLASS(Abstract,config=Game)
class AIMODULE_API UBTNode : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual UWorld* GetWorld() const override;

	/** fill in data about tree structure */
	void InitializeNode(UBTCompositeNode* InParentNode, uint16 InExecutionIndex, uint16 InMemoryOffset, uint8 InTreeDepth);

	/** initialize any asset related data */
	virtual void InitializeFromAsset(UBehaviorTree& Asset);
	
	/** initialize memory block */
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const;

	/** cleanup memory block */
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const;

	/** gathers description of all runtime parameters */
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const;

	/** size of instance memory */
	virtual uint16 GetInstanceMemorySize() const;

	/** called when node instance is added to tree */
	virtual void OnInstanceCreated(UBehaviorTreeComponent& OwnerComp);

	/** called when node instance is removed from tree */
	virtual void OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp);

	/** called on creating subtree to set up memory and instancing */
	void InitializeInSubtree(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, int32& NextInstancedIndex, EBTMemoryInit::Type InitType) const;

	/** called on removing subtree to cleanup memory */
	void CleanupInSubtree(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const;

	/** size of special, hidden memory block for internal mechanics */
	virtual uint16 GetSpecialMemorySize() const;

#if USE_BEHAVIORTREE_DEBUGGER
	/** fill in data about execution order */
	void InitializeExecutionOrder(UBTNode* NextNode);

	/** @return next node in execution order */
	UBTNode* GetNextNode() const;
#endif

	template<typename T>
	T* GetNodeMemory(FBehaviorTreeSearchData& SearchData) const;

	template<typename T>
	const T* GetNodeMemory(const FBehaviorTreeSearchData& SearchData) const;

	template<typename T>
	T* GetNodeMemory(FBehaviorTreeInstance& BTInstance) const;

	template<typename T>
	const T* GetNodeMemory(const FBehaviorTreeInstance& BTInstance) const;

	/** get special memory block used for hidden shared data (e.g. node instancing) */
	template<typename T>
	T* GetSpecialNodeMemory(uint8* NodeMemory) const;

	/** @return parent node */
	UBTCompositeNode* GetParentNode() const;

	/** @return name of node */
	FString GetNodeName() const;

	/** @return execution index */
	uint16 GetExecutionIndex() const;

	/** @return memory offset */
	uint16 GetMemoryOffset() const;

	/** @return depth in tree */
	uint8 GetTreeDepth() const;

	/** sets bIsInjected flag, do NOT call this function unless you really know what you are doing! */
	void MarkInjectedNode();

	/** @return true if node was injected by subtree */
	bool IsInjected() const;

	/** sets bCreateNodeInstance flag, do NOT call this function on already pushed tree instance! */
	void ForceInstancing(bool bEnable);

	/** @return true if node wants to be instanced */
	bool HasInstance() const;

	/** @return true if this object is instanced node */
	bool IsInstanced() const;

	/** @return tree asset */
	UBehaviorTree* GetTreeAsset() const;

	/** @return blackboard asset */
	UBlackboardData* GetBlackboardAsset() const;

	/** @return node instance if bCreateNodeInstance was set */
	UBTNode* GetNodeInstance(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const;
	UBTNode* GetNodeInstance(FBehaviorTreeSearchData& SearchData) const;

	/** @return string containing description of this node instance with all relevant runtime values */
	FString GetRuntimeDescription(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity) const;

	/** @return string containing description of this node with all setup values */
	virtual FString GetStaticDescription() const;

#if WITH_EDITOR
	/** Get the name of the icon used to display this node in the editor */
	virtual FName GetNodeIconName() const;

	/** Get whether this node is using a blueprint for its logic */
	virtual bool UsesBlueprint() const;
#endif

	/** Gets called only for instanced nodes(bCreateNodeInstance == true). In practive overriden by BP-implemented BT nodes */
	virtual void SetOwner(AActor* ActorOwner) {}

	/** node name */
	UPROPERTY(Category=Description, EditAnywhere)
	FString NodeName;
	
private:

	/** source asset */
	UPROPERTY()
	UBehaviorTree* TreeAsset;

	/** parent node */
	UPROPERTY()
	UBTCompositeNode* ParentNode;

#if USE_BEHAVIORTREE_DEBUGGER
	/** next node in execution order */
	UBTNode* NextExecutionNode;
#endif

	/** depth first index (execution order) */
	uint16 ExecutionIndex;

	/** instance memory offset */
	uint16 MemoryOffset;

	/** depth in tree */
	uint8 TreeDepth;

protected:

	/** if set, node will be instanced instead of using memory block and template shared with all other BT components */
	uint8 bCreateNodeInstance : 1;

	/** set automatically for node instances */
	uint8 bIsInstanced : 1;

	/** if set, node is injected by subtree */
	uint8 bIsInjected : 1;

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
public:
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	virtual void InitializeMemory(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const;
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	virtual void CleanupMemory(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const;
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const;
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	virtual void OnInstanceCreated(UBehaviorTreeComponent* OwnerComp);
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	virtual void OnInstanceDestroyed(UBehaviorTreeComponent* OwnerComp);
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	void InitializeInSubtree(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, int32& NextInstancedIndex, EBTMemoryInit::Type InitType) const;
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	void CleanupInSubtree(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const;
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	UBTNode* GetNodeInstance(const UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const;
	DEPRECATED(4.7, "This version is deprecated. Please use the one taking reference to UBehaviorTreeComponent rather than a pointer.")
	FString GetRuntimeDescription(const UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity) const;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE UBehaviorTree* UBTNode::GetTreeAsset() const
{
	return TreeAsset;
}

FORCEINLINE UBTCompositeNode* UBTNode::GetParentNode() const
{
	return ParentNode;
}

#if USE_BEHAVIORTREE_DEBUGGER
FORCEINLINE UBTNode* UBTNode::GetNextNode() const
{
	return NextExecutionNode;
}
#endif

FORCEINLINE uint16 UBTNode::GetExecutionIndex() const
{
	return ExecutionIndex;
}

FORCEINLINE uint16 UBTNode::GetMemoryOffset() const
{
	return MemoryOffset;
}

FORCEINLINE uint8 UBTNode::GetTreeDepth() const
{
	return TreeDepth;
}

FORCEINLINE void UBTNode::MarkInjectedNode()
{
	bIsInjected = true;
}

FORCEINLINE bool UBTNode::IsInjected() const
{
	return bIsInjected;
}

FORCEINLINE void UBTNode::ForceInstancing(bool bEnable)
{
	// allow only in not initialized trees, side effect: root node always blocked
	check(ParentNode == NULL);

	bCreateNodeInstance = bEnable;
}

FORCEINLINE bool UBTNode::HasInstance() const
{
	return bCreateNodeInstance;
}

FORCEINLINE bool UBTNode::IsInstanced() const
{
	return bIsInstanced;
}

template<typename T>
T* UBTNode::GetNodeMemory(FBehaviorTreeSearchData& SearchData) const
{
	return GetNodeMemory<T>(SearchData.OwnerComp.InstanceStack[SearchData.OwnerComp.GetActiveInstanceIdx()]);
}

template<typename T>
const T* UBTNode::GetNodeMemory(const FBehaviorTreeSearchData& SearchData) const
{
	return GetNodeMemory<T>(SearchData.OwnerComp.InstanceStack[SearchData.OwnerComp.GetActiveInstanceIdx()]);
}

template<typename T>
T* UBTNode::GetNodeMemory(FBehaviorTreeInstance& BTInstance) const
{
	return (T*)(BTInstance.InstanceMemory.GetData() + MemoryOffset);
}

template<typename T>
const T* UBTNode::GetNodeMemory(const FBehaviorTreeInstance& BTInstance) const
{
	return (const T*)(BTInstance.InstanceMemory.GetData() + MemoryOffset);
}

template<typename T>
T* UBTNode::GetSpecialNodeMemory(uint8* NodeMemory) const
{
	const int32 SpecialMemorySize = GetSpecialMemorySize();
	return SpecialMemorySize ? (T*)(NodeMemory - ((SpecialMemorySize + 3) & ~3)) : nullptr;
}
