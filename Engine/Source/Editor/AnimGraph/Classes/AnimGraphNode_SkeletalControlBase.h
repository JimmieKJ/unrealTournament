// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_SkeletalControlBase.generated.h"

struct HActor;

/**
 * This is the base class for the 'source version' of all skeletal control animation graph nodes
 * (nodes that manipulate the pose rather than playing animations to create a pose or blending between poses)
 *
 * Concrete subclasses should contain a member struct derived from FAnimNode_SkeletalControlBase
 */
UCLASS(Abstract)
class ANIMGRAPH_API UAnimGraphNode_SkeletalControlBase : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	virtual FString GetNodeCategory() const override;
	virtual void CreateOutputPins() override;
	virtual void ValidateAnimNodePostCompile(FCompilerResultsLog& MessageLog, UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex) override;
	// End of UAnimGraphNode_Base interface

	// Draw function for supporting visualization
	virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent * PreviewSkelMeshComp) const {}
	// Canvas draw function to draw to viewport
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, USkeletalMeshComponent * PreviewSkelMeshComp) const {}
	// Function to collect strings from nodes to display in the viewport.
	// Use this rather than DrawCanvas when adding general text to the viewport.
	virtual void GetOnScreenDebugInfo(TArray<FText>& DebugInfo, USkeletalMeshComponent* PreviewSkelMeshComp) const {}

	/**
	 * methods related to widget control
	 */
	virtual FVector GetWidgetLocation(const USkeletalMeshComponent* SkelComp, struct FAnimNode_SkeletalControlBase* AnimNode)
	{
		return FVector::ZeroVector;
	}
	// to keep data consistency between anim nodes 
	virtual void CopyNodeDataTo(FAnimNode_Base* OutAnimNode){}
	virtual void CopyNodeDataFrom(const FAnimNode_Base* NewAnimNode){}

	/** Are we currently showing this pin */
	bool IsPinShown(const FString& PinName) const;

	// Returns the coordinate system that should be used for this bone
	virtual int32 GetWidgetCoordinateSystem(const USkeletalMeshComponent* SkelComp);

	// return current widget mode this anim graph node supports
	virtual int32 GetWidgetMode(const USkeletalMeshComponent* SkelComp);
	// called when the user changed widget mode by pressing "Space" key
	virtual int32 ChangeToNextWidgetMode(const USkeletalMeshComponent* SkelComp, int32 CurWidgetMode);
	// called when the user set widget mode directly, returns true if InWidgetMode is available
	virtual bool SetWidgetMode(const USkeletalMeshComponent* SkelComp, int32 InWidgetMode) { return false; }

	// 
	virtual FName FindSelectedBone();

	// if anim graph node needs other actors to select other bones, move actor's positions when this is called
	virtual void MoveSelectActorLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* InAnimNode) {}

	virtual bool IsActorClicked(HActor* ActorHitProxy) { return false; }
	virtual void ProcessActorClick(HActor* ActorHitProxy) {}
	// if it has select-actors, should hide all actors when de-select is called  
	virtual void DeselectActor(USkeletalMeshComponent* SkelComp){}

	// called when the widget is dragged in translation mode
	virtual void DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode) {}
	// called when the widget is dragged in rotation mode
	virtual void DoRotation(const USkeletalMeshComponent* SkelComp, FRotator& Rotation, FAnimNode_Base* InOutAnimNode) {}
	// called when the widget is dragged in scale mode
	virtual void DoScale(const USkeletalMeshComponent* SkelComp, FVector& Scale, FAnimNode_Base* InOutAnimNode) {}

	/** Try to find the preview control node instance for this anim graph node */
	FAnimNode_SkeletalControlBase* FindDebugAnimNode(USkeletalMeshComponent * PreviewSkelMeshComp) const;

protected:
	// Returns the short descriptive name of the controller
	virtual FText GetControllerDescription() const;

	/**
	* helper functions for bone control preview
	*/
	// local conversion function for drawing
	void ConvertToComponentSpaceTransform(const USkeletalMeshComponent* SkelComp, const FTransform & InTransform, FTransform & OutCSTransform, int32 BoneIndex, EBoneControlSpace Space) const;
	// convert drag vector in component space to bone space 
	FVector ConvertCSVectorToBoneSpace(const USkeletalMeshComponent* SkelComp, FVector& InCSVector, FCSPose<FCompactHeapPose>& MeshBases, const FName& BoneName, const EBoneControlSpace Space);
	// convert rotator in component space to bone space 
	FQuat ConvertCSRotationToBoneSpace(const USkeletalMeshComponent* SkelComp, FRotator& InCSRotator, FCSPose<FCompactHeapPose>& MeshBases, const FName& BoneName, const EBoneControlSpace Space);
	// convert widget location according to bone control space
	FVector ConvertWidgetLocation(const USkeletalMeshComponent* InSkelComp, FCSPose<FCompactHeapPose>& InMeshBases, const FName& BoneName, const FVector& InLocation, const EBoneControlSpace Space);
	// set literal value for FVector
	void SetDefaultValue(const FString& InDefaultValueName, const FVector& InValue);
	// get literal value for vector
	void GetDefaultValue(const FString& UpdateDefaultValueName, FVector& OutVec);

	void GetDefaultValue(const FString& PropName, FRotator& OutValue)
	{
		FVector Value;
		GetDefaultValue(PropName, Value);
		OutValue.Pitch = Value.X;
		OutValue.Yaw = Value.Y;
		OutValue.Roll = Value.Z;
	}

	template<class ValueType>
	ValueType GetNodeValue(const FString& PropName, const ValueType& CompileNodeValue)
	{
		if (IsPinShown(PropName))
		{
			ValueType Val;
			GetDefaultValue(PropName, Val);
			return Val;
		}
		return CompileNodeValue;
	}

	void SetDefaultValue(const FString& PropName, const FRotator& InValue)
	{
		FVector VecValue(InValue.Pitch, InValue.Yaw, InValue.Roll);
		SetDefaultValue(PropName, VecValue);
	}

	template<class ValueType>
	void SetNodeValue(const FString& PropName, ValueType& CompileNodeValue, const ValueType& InValue)
	{
		if (IsPinShown(PropName))
		{
			SetDefaultValue(PropName, InValue);
		}
		CompileNodeValue = InValue;
	}

	virtual const FAnimNode_SkeletalControlBase* GetNode() const PURE_VIRTUAL(UAnimGraphNode_SkeletalControlBase::GetNode, return nullptr;);
};
