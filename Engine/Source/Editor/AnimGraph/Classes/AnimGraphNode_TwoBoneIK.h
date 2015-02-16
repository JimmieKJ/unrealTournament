// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/BoneControllers/AnimNode_TwoBoneIK.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTitleTextTable
#include "AnimGraphNode_TwoBoneIK.generated.h"

// actor class used for bone selector
#define ABoneSelectActor ATargetPoint

UCLASS(MinimalAPI)
class UAnimGraphNode_TwoBoneIK : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_TwoBoneIK Node;

	// just for refreshing UIs when bone space was changed
	static TSharedPtr<class FTwoBoneIKDelegate> TwoBoneIKDelegate;

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	// End of UEdGraphNode interface

	void UpdateEffectorLocationSpace(class IDetailLayoutBuilder& DetailBuilder);
	// UAnimGraphNode_Base interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End of UAnimGraphNode_Base interface

	// UAnimGraphNode_SkeletalControlBase interface
	ANIMGRAPH_API virtual void Draw( FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const override;

	ANIMGRAPH_API virtual FVector GetWidgetLocation(const USkeletalMeshComponent* SkelComp, struct FAnimNode_SkeletalControlBase* AnimNode) override;

	ANIMGRAPH_API virtual void	CopyNodeDataFrom(const FAnimNode_Base* NewAnimNode) override;

	ANIMGRAPH_API virtual int32 GetWidgetMode(const USkeletalMeshComponent* SkelComp) override;

	ANIMGRAPH_API virtual void MoveSelectActorLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* AnimNode) override;
	ANIMGRAPH_API virtual FName FindSelectedBone() override;
	ANIMGRAPH_API virtual bool IsActorClicked(HActor* ActorHitProxy) override;
	ANIMGRAPH_API virtual void ProcessActorClick(HActor* ActorHitProxy) override;
	ANIMGRAPH_API virtual void DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode) override;
	ANIMGRAPH_API virtual void	CopyNodeDataTo(FAnimNode_Base* AnimNode) override;
	ANIMGRAPH_API virtual void	DeselectActor(USkeletalMeshComponent* SkelComp) override;
	// End of UAnimGraphNode_SkeletalControlBase interface

	enum BoneSelectModeType
	{
		BSM_EndEffector,
		BSM_JointTarget,
		BSM_Max
	};

	// to switch select mode between end effector and joint target
	BoneSelectModeType BoneSelectMode;

	TWeakObjectPtr<class ABoneSelectActor> BoneSelectActor;

	IDetailLayoutBuilder* DetailLayout;

protected:
	// UAnimGraphNode_SkeletalControlBase interface
	virtual FText GetControllerDescription() const override;
	// End of UAnimGraphNode_SkeletalControlBase interface

	// local conversion function for drawing
	void DrawTargetLocation(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, USkeleton * Skeleton, EBoneControlSpace SpaceBase, FName SpaceBoneName, const FVector & TargetLocation, const FColor & TargetColor, const FColor & BoneColor) const;

	// make Pins showed / hidden by options
	bool SetPinsVisibility(bool bShow);

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTitleTextTable CachedNodeTitles;
};
