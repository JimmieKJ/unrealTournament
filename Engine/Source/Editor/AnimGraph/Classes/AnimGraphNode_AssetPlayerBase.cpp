// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "Animation/AnimComposite.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_BlendSpaceEvaluator.h"
#include "AnimGraphNode_PoseBlendNode.h"
#include "AnimGraphNode_PoseByName.h"

void UAnimGraphNode_AssetPlayerBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		// if pin has no connections, we may need to restore the default
		if (Pin->LinkedTo.Num() == 0)
		{
			UObject* Asset = GetAssetReferenceForPinRestoration();
			if (Asset != nullptr)
			{
				Pin->DefaultObject = Asset;
			}
		}

		// recache visualization now an asset pin's connection is changed
		if (const UEdGraphSchema* Schema = GetSchema())
		{
			Schema->ForceVisualizationCacheClear();
		}
	}
}

void UAnimGraphNode_AssetPlayerBase::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		// recache visualization now an asset pin's default value has changed
		if (const UEdGraphSchema* Schema = GetSchema())
		{
			Schema->ForceVisualizationCacheClear();
		}
	}
}

void UAnimGraphNode_AssetPlayerBase::SetAssetReferenceForPinRestoration(UObject* InAsset)
{
	AssetReferenceForPinRestoration = InAsset;
}

UObject* UAnimGraphNode_AssetPlayerBase::GetAssetReferenceForPinRestoration()
{
	return AssetReferenceForPinRestoration.TryLoad();
}

bool IsAimOffsetBlendSpace(const UClass* BlendSpaceClass)
{
	return  BlendSpaceClass->IsChildOf(UAimOffsetBlendSpace::StaticClass()) ||
		BlendSpaceClass->IsChildOf(UAimOffsetBlendSpace1D::StaticClass());
}

UClass* GetNodeClassForAsset(const UClass* AssetClass)
{
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()))
	{
		return UAnimGraphNode_SequencePlayer::StaticClass();
	}
	else if (AssetClass->IsChildOf(UBlendSpaceBase::StaticClass()))
	{
		if (IsAimOffsetBlendSpace(AssetClass))
		{
			return UAnimGraphNode_RotationOffsetBlendSpace::StaticClass();
		}
		else
		{
			return UAnimGraphNode_BlendSpacePlayer::StaticClass();
		}
	}
	else if (AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return UAnimGraphNode_SequencePlayer::StaticClass();
	}
	else if (AssetClass->IsChildOf(UPoseAsset::StaticClass()))
	{
		return UAnimGraphNode_PoseBlendNode::StaticClass();
	}
	return nullptr;
}

bool SupportNodeClassForAsset(const UClass* AssetClass, const UClass* NodeClass)
{
	// we don't want montage to show up, so not checking AnimSequenceBase
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) || AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return (UAnimGraphNode_SequencePlayer::StaticClass() == NodeClass || UAnimGraphNode_SequenceEvaluator::StaticClass() == NodeClass);
	}
	else if (AssetClass->IsChildOf(UBlendSpaceBase::StaticClass()))
	{
		if (IsAimOffsetBlendSpace(AssetClass))
		{
			return (UAnimGraphNode_RotationOffsetBlendSpace::StaticClass() == NodeClass);
		}
		else
		{
			return (UAnimGraphNode_BlendSpacePlayer::StaticClass() == NodeClass || UAnimGraphNode_BlendSpaceEvaluator::StaticClass() == NodeClass);
		}
	}
	else if (AssetClass->IsChildOf(UPoseAsset::StaticClass()))
	{
		return (UAnimGraphNode_PoseBlendNode::StaticClass() == NodeClass || UAnimGraphNode_PoseByName::StaticClass() == NodeClass);
	}
	
	return false;
}

