// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "GraphEditorActions.h"
#include "CompilerResultsLog.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Animation/AnimationSettings.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_RotationOffsetBlendSpace

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_RotationOffsetBlendSpace::UAnimGraphNode_RotationOffsetBlendSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_RotationOffsetBlendSpace::GetTooltipText() const
{
	// FText::Format() is slow, so we utilize the cached list title
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_RotationOffsetBlendSpace::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.BlendSpace == nullptr)
	{
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			return LOCTEXT("RotationOffsetBlend_NONE_ListTitle", "AimOffset '(None)'");
		}
		else
		{
			return LOCTEXT("RotationOffsetBlend_NONE_Title", "(None)\nAimOffset");
		}
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		const FText BlendSpaceName = FText::FromString(Node.BlendSpace->GetName());

		FFormatNamedArguments Args;
		Args.Add(TEXT("BlendSpaceName"), BlendSpaceName);

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AimOffsetListTitle", "AimOffset '{BlendSpaceName}'"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AimOffsetFullTitle", "{BlendSpaceName}\nAimOffset"), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];
}

void UAnimGraphNode_RotationOffsetBlendSpace::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	struct GetMenuActions_Utils
	{
		static void SetNodeBlendSpace(UEdGraphNode* NewNode, bool /*bIsTemplateNode*/, TWeakObjectPtr<UBlendSpaceBase> BlendSpace)
		{
			UAnimGraphNode_RotationOffsetBlendSpace* BlendSpaceNode = CastChecked<UAnimGraphNode_RotationOffsetBlendSpace>(NewNode);
			BlendSpaceNode->Node.BlendSpace = BlendSpace.Get();
		}

		static UBlueprintNodeSpawner* MakeBlendSpaceAction(TSubclassOf<UEdGraphNode> const NodeClass, const UBlendSpaceBase* BlendSpace)
		{
			UBlueprintNodeSpawner* NodeSpawner = nullptr;

			bool const bIsAimOffset = BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) ||
				BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
			if (bIsAimOffset)
			{
				NodeSpawner = UBlueprintNodeSpawner::Create(NodeClass);
				check(NodeSpawner != nullptr);

				TWeakObjectPtr<UBlendSpaceBase> BlendSpacePtr = BlendSpace;
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(GetMenuActions_Utils::SetNodeBlendSpace, BlendSpacePtr);
			}
			return NodeSpawner;
		}
	};

	if (const UObject* RegistrarTarget = ActionRegistrar.GetActionKeyFilter())
	{
		if (const UBlendSpaceBase* TargetBlendSpace = Cast<UBlendSpaceBase>(RegistrarTarget))
		{
			if (UBlueprintNodeSpawner* NodeSpawner = GetMenuActions_Utils::MakeBlendSpaceAction(GetClass(), TargetBlendSpace))
			{
				ActionRegistrar.AddBlueprintAction(TargetBlendSpace, NodeSpawner);
			}
		}
		// else, the Blueprint database is specifically looking for actions pertaining to something different (not a BlendSpace asset)
	}
	else
	{
		UClass* NodeClass = GetClass();
		for (TObjectIterator<UBlendSpaceBase> BlendSpaceIt; BlendSpaceIt; ++BlendSpaceIt)
		{
			UBlendSpaceBase* BlendSpace = *BlendSpaceIt;
			if (UBlueprintNodeSpawner* NodeSpawner = GetMenuActions_Utils::MakeBlendSpaceAction(NodeClass, BlendSpace))
			{
				ActionRegistrar.AddBlueprintAction(BlendSpace, NodeSpawner);
			}
		}
	}
}

FBlueprintNodeSignature UAnimGraphNode_RotationOffsetBlendSpace::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddSubObject(Node.BlendSpace);

	return NodeSignature;
}

void UAnimGraphNode_RotationOffsetBlendSpace::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Asset))
	{
		Node.BlendSpace = BlendSpace;
	}
}

void UAnimGraphNode_RotationOffsetBlendSpace::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	if (Node.BlendSpace == NULL)
	{
		MessageLog.Error(TEXT("@@ references an unknown blend space"), this);
	}
	else if (Cast<UAimOffsetBlendSpace>(Node.BlendSpace) == NULL &&
			 Cast<UAimOffsetBlendSpace1D>(Node.BlendSpace) == NULL)
	{
		MessageLog.Error(TEXT("@@ references an invalid blend space (one that is not an aim offset)"), this);
	}
	else
	{
		USkeleton* BlendSpaceSkeleton = Node.BlendSpace->GetSkeleton();
		if (BlendSpaceSkeleton && // if blend space doesn't have skeleton, it might be due to blend space not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!BlendSpaceSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references blendspace that uses different skeleton @@"), this, BlendSpaceSkeleton);
		}
	}

	if (UAnimationSettings::Get()->bEnablePerformanceLog)
	{
		if (Node.LODThreshold < 0)
		{
			MessageLog.Warning(TEXT("@@ contains no LOD Threshold."), this);
		}
	}
}

void UAnimGraphNode_RotationOffsetBlendSpace::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to single frame
		Context.MenuBuilder->BeginSection("AnimGraphNodeBlendSpacePlayer", NSLOCTEXT("A3Nodes", "BlendSpaceHeading", "Blend Space"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
		}
		Context.MenuBuilder->EndSection();
	}
}

void UAnimGraphNode_RotationOffsetBlendSpace::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if(Node.BlendSpace)
	{
		HandleAnimReferenceCollection(Node.BlendSpace, AnimationAssets);
	}
}

void UAnimGraphNode_RotationOffsetBlendSpace::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.BlendSpace, AnimAssetReplacementMap);
}

#undef LOCTEXT_NAMESPACE