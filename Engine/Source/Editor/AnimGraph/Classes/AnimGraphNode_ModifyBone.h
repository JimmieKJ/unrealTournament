// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTitleTextTable
#include "AnimGraphNode_ModifyBone.generated.h"

UCLASS(meta=(Keywords = "Modify Transform"))
class ANIMGRAPH_API UAnimGraphNode_ModifyBone : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_ModifyBone Node;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	// End of UEdGraphNode interface

protected:
	// UAnimGraphNode_Base interface
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	// End of UAnimGraphNode_Base interface

	// UAnimGraphNode_SkeletalControlBase interface
	virtual FText GetControllerDescription() const override;
	virtual int32 GetWidgetCoordinateSystem(const USkeletalMeshComponent* SkelComp) override;
	virtual FVector GetWidgetLocation(const USkeletalMeshComponent* SkelComp, struct FAnimNode_SkeletalControlBase* AnimNode) override;
	virtual int32 GetWidgetMode(const USkeletalMeshComponent* SkelComp) override;
	virtual int32 ChangeToNextWidgetMode(const USkeletalMeshComponent* SkelComp, int32 InCurWidgetMode) override;
	virtual bool SetWidgetMode(const USkeletalMeshComponent* SkelComp, int32 InWidgetMode) override;
	virtual FName FindSelectedBone() override;
	virtual void DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode) override;
	virtual void DoRotation(const USkeletalMeshComponent* SkelComp, FRotator& Rotation, FAnimNode_Base* InOutAnimNode) override;
	virtual void DoScale(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode) override;
	virtual void CopyNodeDataTo(FAnimNode_Base* OutAnimNode) override;
	virtual void CopyNodeDataFrom(const FAnimNode_Base* InNewAnimNode) override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	// End of UAnimGraphNode_SkeletalControlBase interface

	// methods to find a valid widget mode for gizmo because doesn't need to show gizmo when the mode is "Ignore"
	int32 FindValidWidgetMode(int32 InWidgetMode);
	EBoneModificationMode GetBoneModificationMode(int32 InWidgetMode);
	int32 GetNextWidgetMode(int32 InWidgetMode);

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTitleTextTable CachedNodeTitles;

	// storing current widget mode 
	int32 CurWidgetMode;
};

