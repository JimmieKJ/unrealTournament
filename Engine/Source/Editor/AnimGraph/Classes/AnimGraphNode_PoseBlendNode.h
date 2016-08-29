// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimNodes/AnimNode_PoseBlendNode.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "AnimGraphNode_PoseBlendNode.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_PoseBlendNode : public UAnimGraphNode_AssetPlayerBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_PoseBlendNode Node;

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	// End of UEdGraphNode

	// UAnimGraphNode_Base interface
	virtual void ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog) override;
	virtual void PreloadRequiredAssets() override;
	// Interface to support transition getter
	virtual bool DoesSupportTimeForTransitionGetter() const override;
	virtual UAnimationAsset* GetAnimationAsset() const override;
	virtual void GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	// End of UAnimGraphNode_Base

	// UK2Node interface
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	// End of UK2Node interface

	virtual void SetAnimationAsset(UAnimationAsset* Asset) override;

private:
	FText GetNodeTitleForPoseAsset(ENodeTitleType::Type TitleType, UPoseAsset* InPoseAsset) const;

private:
	static FText GetTitleGivenAssetInfo(const FText& AssetName);

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;

	/** Used for filtering in the Blueprint context menu when the pose asset this node uses is unloaded */
	FString UnloadedSkeletonName;
};
