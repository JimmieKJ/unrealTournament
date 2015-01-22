// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimBlueprint.h"
#include "AnimBlueprintGeneratedClass.h"
#include "AnimInstance.h"
#include "AnimationRuntime.h"
#include "AnimNodeBase.generated.h"


// Base class for update/evaluate contexts
struct FAnimationBaseContext
{
public:
	UAnimInstance* AnimInstance;
protected:
	FAnimationBaseContext(UAnimInstance* InInstance)
		: AnimInstance(InInstance)
	{
	}

public:
	// Get the Blueprint Generated Class associated with this context, if there is one.
	// Note: This can return NULL, so check the result.
	UAnimBlueprintGeneratedClass* GetAnimBlueprintClass() const
	{
		return Cast<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass());
	}

#if WITH_EDITORONLY_DATA
	// Get the AnimBlueprint associated with this context, if there is one.
	// Note: This can return NULL, so check the result.
	UAnimBlueprint* GetAnimBlueprint() const
	{
		if (UAnimBlueprintGeneratedClass* Class = GetAnimBlueprintClass())
		{
			return Cast<UAnimBlueprint>(Class->ClassGeneratedBy);
		}
		return NULL;
	}
#endif //WITH_EDITORONLY_DATA
};


// Initialization context passed around during animation tree initialization
struct FAnimationInitializeContext : public FAnimationBaseContext
{
public:
	FAnimationInitializeContext(UAnimInstance* InInstance)
		: FAnimationBaseContext(InInstance)
	{
	}
};

// Context passed around when RequiredBones array changed and cached bones indices have to be refreshed.
// (RequiredBones array changed because of an LOD switch for example)
struct FAnimationCacheBonesContext : public FAnimationBaseContext
{
public:
	FAnimationCacheBonesContext(UAnimInstance* InInstance)
		: FAnimationBaseContext(InInstance)
	{
	}
};

// Update context passed around during animation tree update
struct FAnimationUpdateContext : public FAnimationBaseContext
{
private:
	float CurrentWeight;
	float DeltaTime;
public:
	FAnimationUpdateContext(UAnimInstance* Instance, float InDeltaTime)
		: FAnimationBaseContext(Instance)
		, CurrentWeight(1.0f)
		, DeltaTime(InDeltaTime)
	{
	}

	FAnimationUpdateContext FractionalWeight(float Multiplier) const
	{
		FAnimationUpdateContext Result(AnimInstance, DeltaTime);
		Result.CurrentWeight = CurrentWeight * Multiplier;
		return Result;
	}

	FAnimationUpdateContext FractionalWeightAndTime(float WeightMultiplier, float TimeMultiplier) const
	{
		FAnimationUpdateContext Result(AnimInstance, DeltaTime * TimeMultiplier);
		Result.CurrentWeight = CurrentWeight * WeightMultiplier;
		return Result;
	}

	// Returns the final blend weight contribution for this stage
	float GetFinalBlendWeight() const { return CurrentWeight; }

	// Returns the delta time for this update, in seconds
	float GetDeltaTime() const { return DeltaTime; }
};


// Evaluation context passed around during animation tree evaluation
struct FPoseContext : public FAnimationBaseContext
{
public:
	FA2Pose Pose;

public:
	// This constructor allocates a new uninitialized pose for the specified anim instance
	FPoseContext(UAnimInstance* InAnimInstance)
		: FAnimationBaseContext(InAnimInstance)
	{
		checkSlow( AnimInstance && AnimInstance->RequiredBones.IsValid() );
		const int32 NumBones = AnimInstance->RequiredBones.GetNumBones();
		Pose.Bones.Empty(NumBones);
		Pose.Bones.AddUninitialized(NumBones);
	}

	// This constructor allocates a new uninitialized pose, copying non-pose state from the source context
	FPoseContext(const FPoseContext& SourceContext)
		: FAnimationBaseContext(SourceContext.AnimInstance)
	{
		checkSlow( AnimInstance && AnimInstance->RequiredBones.IsValid() );
		const int32 NumBones = AnimInstance->RequiredBones.GetNumBones();
		Pose.Bones.Empty(NumBones);
		Pose.Bones.AddUninitialized(NumBones);
	}

	void ResetToRefPose()
	{
		checkSlow( AnimInstance && AnimInstance->RequiredBones.IsValid() );
		FAnimationRuntime::FillWithRefPose(Pose.Bones, AnimInstance->RequiredBones);
	}

	void ResetToIdentity()
	{
		checkSlow( AnimInstance && AnimInstance->RequiredBones.IsValid() );
		FAnimationRuntime::InitializeTransform(AnimInstance->RequiredBones, Pose.Bones);
	}

	bool ContainsNaN() const;
	bool IsNormalized() const;
};


// Evaluation context passed around during animation tree evaluation
struct FComponentSpacePoseContext : public FAnimationBaseContext
{
public:
	FA2CSPose Pose;

public:
	// This constructor allocates a new uninitialized pose for the specified anim instance
	FComponentSpacePoseContext(UAnimInstance* InAnimInstance)
		: FAnimationBaseContext(InAnimInstance)
	{
		// No need to initialize, done through FA2CSPose::AllocateLocalPoses
	}

	// This constructor allocates a new uninitialized pose, copying non-pose state from the source context
	FComponentSpacePoseContext(const FComponentSpacePoseContext& SourceContext)
		: FAnimationBaseContext(SourceContext.AnimInstance)
	{
		// No need to initialize, done through FA2CSPose::AllocateLocalPoses
	}

	void ResetToRefPose()
	{
		checkSlow( AnimInstance && AnimInstance->RequiredBones.IsValid() );
		Pose.AllocateLocalPoses(AnimInstance->RequiredBones, AnimInstance->RequiredBones.GetRefPoseArray());
	}

	bool ContainsNaN() const;
	bool IsNormalized() const;
};

struct FNodeDebugData
{
private:
	struct DebugItem
	{
		DebugItem(FString Data, bool bInPoseSource) : DebugData(Data), bPoseSource(bInPoseSource) {}
		
		// This node items debug text to display
		FString DebugData;

		// Whether we are supplying a pose instead of modifying one (e.g. an playing animation)
		bool	bPoseSource;

		// Nodes that we are connected to
		TArray<FNodeDebugData> ChildNodeChain;
	};

	// This nodes final contribution weight (based on its own weight and the weight of its parents)
	float			AbsoluteWeight;

	// Nodes that we are dependent on
	TArray<DebugItem> NodeChain;

public:
	struct FFlattenedDebugData
	{
		FFlattenedDebugData(FString Line, float AbsWeight, int32 InIndent, int32 InChainID, bool bInPoseSource) : DebugLine(Line), AbsoluteWeight(AbsWeight), Indent(InIndent), ChainID(InChainID), bPoseSource(bInPoseSource){}
		FString DebugLine;
		float	AbsoluteWeight;
		int32	Indent;
		int32	ChainID;
		bool	bPoseSource;

		bool IsOnActiveBranch() { return AbsoluteWeight > ZERO_ANIMWEIGHT_THRESH; }
	};

	FNodeDebugData(const class UAnimInstance* InAnimInstance) : AbsoluteWeight(1.f), AnimInstance(InAnimInstance) {}
	FNodeDebugData(const class UAnimInstance* InAnimInstance, const float AbsWeight) : AbsoluteWeight(AbsWeight), AnimInstance(InAnimInstance) {}

	void			AddDebugItem(FString DebugData, bool bPoseSource = false);
	FNodeDebugData&	BranchFlow(float BranchWeight);

	template<class Type>
	FString GetNodeName(Type* Node)
	{
		return FString::Printf(TEXT("%s<W:%.1f%%>"), *Node->StaticStruct()->GetName(), AbsoluteWeight*100.f);
	}

	void GetFlattenedDebugData(TArray<FFlattenedDebugData>& FlattenedDebugData, int32 Indent, int32& ChainID);

	TArray<FFlattenedDebugData> GetFlattenedDebugData()
	{
		TArray<FFlattenedDebugData> Data;
		int32 ChainID = 0;
		GetFlattenedDebugData(Data, 0, ChainID);
		return Data;
	}

	// Anim instance that we are generating debug data for
	const class UAnimInstance* AnimInstance;
};

// The display mode of editable values on an animation node
UENUM()
namespace EPinHidingMode
{
	enum Type
	{
		// Never show this property as a pin, it is only editable in the details panel (default for everything but FPoseLink properties)
		NeverAsPin,

		// Hide this property by default, but allow the user to expose it as a pin via the details panel
		PinHiddenByDefault,

		// Show this property as a pin by default, but allow the user to hide it via the details panel
		PinShownByDefault,

		// Always show this property as a pin; it never makes sense to edit it in the details panel (default for FPoseLink properties)
		AlwaysAsPin
	};
}

// A pose link to another node
USTRUCT()
struct ENGINE_API FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

	// Serialized link ID, used to build the non-serialized pointer map
	UPROPERTY()
	int32 LinkID;

#if WITH_EDITORONLY_DATA
	// The source link ID, used for debug visualization
	UPROPERTY()
	int32 SourceLinkID;
#endif

protected:
	// The non serialized node pointer
	struct FAnimNode_Base* LinkedNode;

	// Flag to prevent reentry when dealing with circular trees
	bool bProcessed;

public:
	FPoseLinkBase()
		: LinkID(INDEX_NONE)
#if WITH_EDITORONLY_DATA
		, SourceLinkID(INDEX_NONE)
#endif
		, LinkedNode(NULL)
		, bProcessed(false)
	{
	}

	// Interface
	void Initialize(const FAnimationInitializeContext& Context);
	void CacheBones(const FAnimationCacheBonesContext& Context) ;
	void Update(const FAnimationUpdateContext& Context);
	void GatherDebugData(FNodeDebugData& DebugData);

	// Try to re-establish the linked node pointer
	void AttemptRelink(const FAnimationBaseContext& Context);
};

// A local-space pose link to another node
USTRUCT()
struct ENGINE_API FPoseLink : public FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

public:
	// Interface
	void Evaluate(FPoseContext& Output);
};

// A component-space pose link to another node
USTRUCT()
struct ENGINE_API FComponentSpacePoseLink : public FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

public:
	// Interface
	void EvaluateComponentSpace(FComponentSpacePoseContext& Output);
};

// An exposed value updater
USTRUCT()
struct FExposedValueHandler
{
	GENERATED_USTRUCT_BODY()

	// The function to call to update associated properties (can be NULL)
	UPROPERTY()
	FName BoundFunction;

	void Execute(const FAnimationBaseContext& Context) const
	{
		if (BoundFunction != NAME_None)
		{
			//@TODO: Should be able to be Checked, or at least produce a warning when it fails
			if (UFunction* Function = Context.AnimInstance->FindFunction(BoundFunction))
			{
				Context.AnimInstance->ProcessEvent(Function, NULL);
			}
		}
	}
};

// To create a new animation node:
//   Create a struct derived from FAnimNode_Base - this is your runtime node
//   Create a class derived from UAnimGraphNode_Base, containing an instance of your runtime as a member - this is your visual/editor-only node

// This is the base of all runtime animation nodes
USTRUCT()
struct ENGINE_API FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	// The default handler for graph-exposed inputs
	UPROPERTY()
	FExposedValueHandler EvaluateGraphExposedInputs;

	// A derived class should implement Initialize, Update, and either Evaluate or EvaluateComponentSpace, but not both of them

	// Interface to implement
	virtual void Initialize(const FAnimationInitializeContext& Context) {}
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) {}
	virtual void Update(const FAnimationUpdateContext& Context) {}
	virtual void Evaluate(FPoseContext& Output) { check(false); }
	virtual void EvaluateComponentSpace(FComponentSpacePoseContext& Output) { check(false); }

	// If a derived anim node should respond to asset overrides, OverrideAsset should be defined to handle changing the asset
	virtual void OverrideAsset(UAnimationAsset* NewAsset) {}

	virtual void GatherDebugData(FNodeDebugData& DebugData)
	{ 
		DebugData.AddDebugItem(TEXT("Non Overriden GatherDebugData")); 
	}
	// End of interface to implement
};
