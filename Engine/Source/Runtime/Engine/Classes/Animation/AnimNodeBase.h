// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimBlueprint.h"
#include "AnimBlueprintGeneratedClass.h"
#include "AnimInstance.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "AnimNodeBase.generated.h"

/** Base class for update/evaluate contexts */
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


/** Initialization context passed around during animation tree initialization */
struct FAnimationInitializeContext : public FAnimationBaseContext
{
public:
	FAnimationInitializeContext(UAnimInstance* InInstance)
		: FAnimationBaseContext(InInstance)
	{
	}
};

/**
 * Context passed around when RequiredBones array changed and cached bones indices have to be refreshed.
 * (RequiredBones array changed because of an LOD switch for example)
 */
struct FAnimationCacheBonesContext : public FAnimationBaseContext
{
public:
	FAnimationCacheBonesContext(UAnimInstance* InInstance)
		: FAnimationBaseContext(InInstance)
	{
	}
};

/** Update context passed around during animation tree update */
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


/** Evaluation context passed around during animation tree evaluation */
struct FPoseContext : public FAnimationBaseContext
{
public:
	FCompactPose	Pose;
	FBlendedCurve	Curve;

public:
	// This constructor allocates a new uninitialized pose for the specified anim instance
	FPoseContext(UAnimInstance* InAnimInstance)
		: FAnimationBaseContext(InAnimInstance)
	{
		Initialize(InAnimInstance);
	}

	// This constructor allocates a new uninitialized pose, copying non-pose state from the source context
	FPoseContext(const FPoseContext& SourceContext)
		: FAnimationBaseContext(SourceContext.AnimInstance)
	{
		Initialize(SourceContext.AnimInstance);
	}

	void Initialize(UAnimInstance* InAnimInstance)
	{
		checkSlow(AnimInstance && AnimInstance->RequiredBones.IsValid());
		Pose.SetBoneContainer(&AnimInstance->RequiredBones);
		Curve.InitFrom(AnimInstance->CurrentSkeleton);
	}

	void ResetToRefPose()
	{
		Pose.ResetToRefPose();	
	}

	void ResetToIdentity()
	{
		Pose.ResetToIdentity();
	}

	bool ContainsNaN() const
	{
		return Pose.ContainsNaN();
	}

	bool IsNormalized() const
	{
		return Pose.IsNormalized();
	}

	FPoseContext& operator=(const FPoseContext& Other)
	{
		if (AnimInstance != Other.AnimInstance)
		{
			Initialize(AnimInstance);
		}

		Pose = Other.Pose;
		Curve = Other.Curve;
		return *this;
	}
};


/** Evaluation context passed around during animation tree evaluation */
struct FComponentSpacePoseContext : public FAnimationBaseContext
{
public:
	FCSPose<FCompactPose>	Pose;
	FBlendedCurve			Curve;

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
		Pose.InitPose(&AnimInstance->RequiredBones);
		Curve.InitFrom(AnimInstance->CurrentSkeleton);
	}

	bool ContainsNaN() const;
	bool IsNormalized() const;
};

struct ENGINE_API FNodeDebugData
{
private:
	struct DebugItem
	{
		DebugItem(FString Data, bool bInPoseSource) : DebugData(Data), bPoseSource(bInPoseSource) {}
		
		/** This node item's debug text to display. */
		FString DebugData;

		/** Whether we are supplying a pose instead of modifying one (e.g. an playing animation). */
		bool bPoseSource;

		/** Nodes that we are connected to. */
		TArray<FNodeDebugData> ChildNodeChain;
	};

	/** This nodes final contribution weight (based on its own weight and the weight of its parents). */
	float AbsoluteWeight;

	/** Nodes that we are dependent on. */
	TArray<DebugItem> NodeChain;

	/** Additional info provided, used in GetNodeName. States machines can provide the state names for the Root Nodes to use for example. */
	FString NodeDescription;

public:
	struct FFlattenedDebugData
	{
		FFlattenedDebugData(FString Line, float AbsWeight, int32 InIndent, int32 InChainID, bool bInPoseSource) : DebugLine(Line), AbsoluteWeight(AbsWeight), Indent(InIndent), ChainID(InChainID), bPoseSource(bInPoseSource){}
		FString DebugLine;
		float AbsoluteWeight;
		int32 Indent;
		int32 ChainID;
		bool bPoseSource;

		bool IsOnActiveBranch() { return AbsoluteWeight > ZERO_ANIMWEIGHT_THRESH; }
	};

	FNodeDebugData(const class UAnimInstance* InAnimInstance) : AbsoluteWeight(1.f), AnimInstance(InAnimInstance) {}
	FNodeDebugData(const class UAnimInstance* InAnimInstance, const float AbsWeight) : AbsoluteWeight(AbsWeight), AnimInstance(InAnimInstance) {}
	FNodeDebugData(const class UAnimInstance* InAnimInstance, const float AbsWeight, FString InNodeDescription) 
		: AbsoluteWeight(AbsWeight)
		, NodeDescription(InNodeDescription)
		, AnimInstance(InAnimInstance) 
	{}

	void AddDebugItem(FString DebugData, bool bPoseSource = false);
	FNodeDebugData& BranchFlow(float BranchWeight, FString InNodeDescription = FString());

	template<class Type>
	FString GetNodeName(Type* Node)
	{
		FString FinalString = FString::Printf(TEXT("%s<W:%.1f%%> %s"), *Node->StaticStruct()->GetName(), AbsoluteWeight*100.f, *NodeDescription);
		NodeDescription.Empty();
		return FinalString;
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
	const UAnimInstance* AnimInstance;
};

/** The display mode of editable values on an animation node. */
UENUM()
namespace EPinHidingMode
{
	enum Type
	{
		/** Never show this property as a pin, it is only editable in the details panel (default for everything but FPoseLink properties). */
		NeverAsPin,

		/** Hide this property by default, but allow the user to expose it as a pin via the details panel. */
		PinHiddenByDefault,

		/** Show this property as a pin by default, but allow the user to hide it via the details panel. */
		PinShownByDefault,

		/** Always show this property as a pin; it never makes sense to edit it in the details panel (default for FPoseLink properties). */
		AlwaysAsPin
	};
}

#define ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG 0

/** A pose link to another node */
USTRUCT()
struct ENGINE_API FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

	/** Serialized link ID, used to build the non-serialized pointer map. */
	UPROPERTY()
	int32 LinkID;

#if WITH_EDITORONLY_DATA
	/** The source link ID, used for debug visualization. */
	UPROPERTY()
	int32 SourceLinkID;
#endif

#if ENABLE_ANIMGRAPH_TRAVERSAL_DEBUG
	FGraphTraversalCounter InitializationCounter;
	FGraphTraversalCounter CachedBonesCounter;
	FGraphTraversalCounter UpdateCounter;
	FGraphTraversalCounter EvaluationCounter;
#endif

protected:
	/** The non serialized node pointer. */
	struct FAnimNode_Base* LinkedNode;

	/** Flag to prevent reentry when dealing with circular trees. */
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

	/** Try to re-establish the linked node pointer. */
	void AttemptRelink(const FAnimationBaseContext& Context);
};

#define ENABLE_ANIMNODE_POSE_DEBUG 0

/** A local-space pose link to another node */
USTRUCT()
struct ENGINE_API FPoseLink : public FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

public:
	// Interface
	void Evaluate(FPoseContext& Output);

#if ENABLE_ANIMNODE_POSE_DEBUG
private:
	// forwarded pose data from the wired node which current node's skeletal control is not applied yet
	FCompactPose CurrentPose;
#endif //#if ENABLE_ANIMNODE_POSE_DEBUG
};

/** A component-space pose link to another node */
USTRUCT()
struct ENGINE_API FComponentSpacePoseLink : public FPoseLinkBase
{
	GENERATED_USTRUCT_BODY()

public:
	// Interface
	void EvaluateComponentSpace(FComponentSpacePoseContext& Output);
};

USTRUCT()
struct FExposedValueCopyRecord
{
	GENERATED_USTRUCT_BODY()

	FExposedValueCopyRecord()
		: Source(nullptr)
		, Dest(nullptr)
	{}

	UPROPERTY()
	UProperty* SourceProperty;

	UPROPERTY()
	int32 SourceArrayIndex;

	UPROPERTY()
	UProperty* DestProperty;

	UPROPERTY()
	int32 DestArrayIndex;

	UPROPERTY()
	int32 Size;

	// Cached source copy ptr
	void* Source;

	// Cached dest copy ptr
	void* Dest;
};

// An exposed value updater
USTRUCT()
struct FExposedValueHandler
{
	GENERATED_USTRUCT_BODY()

	// The function to call to update associated properties (can be NULL)
	UPROPERTY()
	FName BoundFunction;

	// Direct data access to property in anim instance
	UPROPERTY()
	TArray<FExposedValueCopyRecord> CopyRecords;

	// funtion pointer if BoundFunction != NAME_None
	UFunction* Function;

	void Initialize(FAnimNode_Base* AnimNode, UAnimInstance* AnimInstance) 
	{
		if (BoundFunction != NAME_None)
		{
			Function = AnimInstance->FindFunction(BoundFunction);
		}
		else
		{
			Function = NULL;
		}

		// initialize copy records
		for(auto& CopyRecord : CopyRecords)
		{
			if(UArrayProperty* SourceArrayProperty = Cast<UArrayProperty>(CopyRecord.SourceProperty))
			{
				FScriptArrayHelper ArrayHelper(SourceArrayProperty, CopyRecord.SourceProperty->ContainerPtrToValuePtr<uint8>(AnimInstance));
				check(ArrayHelper.IsValidIndex(CopyRecord.SourceArrayIndex));
				CopyRecord.Source = ArrayHelper.GetRawPtr(CopyRecord.SourceArrayIndex);
			}
			else
			{
				CopyRecord.Source = CopyRecord.SourceProperty->ContainerPtrToValuePtr<uint8>(AnimInstance, CopyRecord.SourceArrayIndex);
			}

			if(UArrayProperty* DestArrayProperty = Cast<UArrayProperty>(CopyRecord.DestProperty))
			{
				FScriptArrayHelper ArrayHelper(DestArrayProperty, CopyRecord.DestProperty->ContainerPtrToValuePtr<uint8>(AnimNode));
				check(ArrayHelper.IsValidIndex(CopyRecord.DestArrayIndex));
				CopyRecord.Dest = ArrayHelper.GetRawPtr(CopyRecord.DestArrayIndex);
			}
			else
			{
				CopyRecord.Dest = CopyRecord.DestProperty->ContainerPtrToValuePtr<uint8>(AnimNode, CopyRecord.DestArrayIndex);
			}
		}
	}

	void Execute(const FAnimationBaseContext& Context) const
	{
		if (Function != nullptr)
		{
			Context.AnimInstance->ProcessEvent(Function, NULL);
		}

		for(const auto& CopyRecord : CopyRecords)
		{
			// if any of these checks fail then it's likely that Initialize has not been called.
			// has new anim node type been added that doesnt call the base class Initialize()?
			checkSlow(CopyRecord.Dest != nullptr);
			checkSlow(CopyRecord.Source != nullptr);
			checkSlow(CopyRecord.Size != 0);
			FMemory::Memcpy(CopyRecord.Dest, CopyRecord.Source, CopyRecord.Size);
		}
	}
};

/**
 * This is the base of all runtime animation nodes
 *
 * To create a new animation node:
 *   Create a struct derived from FAnimNode_Base - this is your runtime node
 *   Create a class derived from UAnimGraphNode_Base, containing an instance of your runtime node as a member - this is your visual/editor-only node
 */
USTRUCT()
struct ENGINE_API FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	// The default handler for graph-exposed inputs
	UPROPERTY()
	FExposedValueHandler EvaluateGraphExposedInputs;

	// A derived class should implement Initialize, Update, and either Evaluate or EvaluateComponentSpace, but not both of them

	// Interface to implement
	virtual void Initialize(const FAnimationInitializeContext& Context);
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) {}
	virtual void Update(const FAnimationUpdateContext& Context) {}
	virtual void Evaluate(FPoseContext& Output) { check(false); }
	virtual void EvaluateComponentSpace(FComponentSpacePoseContext& Output) { check(false); }

	// If a derived anim node should respond to asset overrides, OverrideAsset should be defined to handle changing the asset
	virtual void OverrideAsset(UAnimationAsset* NewAsset) {}

	virtual void GatherDebugData(FNodeDebugData& DebugData)
	{ 
		DebugData.AddDebugItem(FString::Printf(TEXT("Non Overriden GatherDebugData! (%s)"), *DebugData.GetNodeName(this)));
	}

	virtual bool CanUpdateInWorkerThread() const { return true; }
	// End of interface to implement

	virtual ~FAnimNode_Base() {}

protected:
	/** return true if eanbled, otherwise, return false. This is utility function that can be used per node level */
	bool IsLODEnabled(UAnimInstance* AnimInstace, int32 InLODThreshold)
	{
		return (InLODThreshold == INDEX_NONE || AnimInstace->GetOwningComponent()->PredictedLODLevel <= InLODThreshold);
	}
};
