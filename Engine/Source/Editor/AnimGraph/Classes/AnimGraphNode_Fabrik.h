// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "BoneControllers/AnimNode_Fabrik.h"
#include "AnimGraphNode_Fabrik.generated.h"

// Editor node for FABRIK IK skeletal controller
UCLASS(MinimalAPI)
class UAnimGraphNode_Fabrik : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_Fabrik Node;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_SkeletalControlBase interface
	ANIMGRAPH_API virtual FVector GetWidgetLocation(const USkeletalMeshComponent* SkelComp, struct FAnimNode_SkeletalControlBase* AnimNode) override;
	ANIMGRAPH_API virtual int32 GetWidgetMode(const USkeletalMeshComponent* SkelComp) override;
	ANIMGRAPH_API virtual FName FindSelectedBone() override;
	ANIMGRAPH_API virtual void DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode) override;

	ANIMGRAPH_API virtual void CopyNodeDataTo(FAnimNode_Base* AnimNode) override;
	ANIMGRAPH_API virtual void CopyNodeDataFrom(const FAnimNode_Base* NewAnimNode) override;
	ANIMGRAPH_API virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent * PreviewSkelMeshComp) const override;
	// End of UAnimGraphNode_SkeletalControlBase interface

protected:
	// UAnimGraphNode_SkeletalControlBase interface
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	// End of UAnimGraphNode_SkeletalControlBase interface
};