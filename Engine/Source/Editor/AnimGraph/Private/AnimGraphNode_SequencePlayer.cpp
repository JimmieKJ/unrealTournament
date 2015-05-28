// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_SequencePlayer.h"

#include "CompilerResultsLog.h"
#include "GraphEditorActions.h"
#include "AssetRegistryModule.h"
#include "AnimationGraphSchema.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintActionFilter.h"
#include "EditorCategoryUtils.h"
#include "BlueprintNodeSpawner.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

/////////////////////////////////////////////////////
// FNewSequencePlayerAction

// Action to add a sequence player node to the graph
struct FNewSequencePlayerAction : public FEdGraphSchemaAction_K2NewNode
{
protected:
	FAssetData AssetInfo;
public:
	FNewSequencePlayerAction(const FAssetData& InAssetInfo, FText Title)
	{
		AssetInfo = InAssetInfo;

		UAnimGraphNode_SequencePlayer* Template = NewObject<UAnimGraphNode_SequencePlayer>();
		NodeTemplate = Template;

		MenuDescription = Title;
		TooltipDescription = TEXT("Evaluates an animation sequence to produce a pose");
		Category = TEXT("Animations");

		// Grab extra keywords
		Keywords = FText::FromName(InAssetInfo.ObjectPath);
	}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override
	{
		UAnimGraphNode_SequencePlayer* SpawnedNode = CastChecked<UAnimGraphNode_SequencePlayer>(FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, Location, bSelectNewNode));
		SpawnedNode->Node.Sequence = Cast<UAnimSequence>(AssetInfo.GetAsset());

		return SpawnedNode;
	}
};

/////////////////////////////////////////////////////
// UAnimGraphNode_SequencePlayer

UAnimGraphNode_SequencePlayer::UAnimGraphNode_SequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_SequencePlayer::PreloadRequiredAssets()
{
	PreloadObject(Node.Sequence);

	Super::PreloadRequiredAssets();
}

FLinearColor UAnimGraphNode_SequencePlayer::GetNodeTitleColor() const
{
	if ((Node.Sequence != NULL) && Node.Sequence->IsValidAdditive())
	{
		return FLinearColor(0.10f, 0.60f, 0.12f);
	}
	else
	{
		return FColor(200, 100, 100);
	}
}

FText UAnimGraphNode_SequencePlayer::GetTooltipText() const
{
	const bool bAdditive = ((Node.Sequence != NULL) && Node.Sequence->IsValidAdditive());
	return GetTitleGivenAssetInfo(FText::FromString(Node.Sequence->GetPathName()), bAdditive);
}

FText UAnimGraphNode_SequencePlayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.Sequence == nullptr)
	{
		return LOCTEXT("SequenceNullTitle", "Play (None)");
	}
	else
	{
		const bool bAdditive = Node.Sequence->IsValidAdditive();
		const FText BasicTitle = GetTitleGivenAssetInfo(FText::FromName(Node.Sequence->GetFName()), bAdditive);

		if (SyncGroup.GroupName == NAME_None)
		{
			return BasicTitle;
		}
		else
		{
			const FText SyncGroupName = FText::FromName(SyncGroup.GroupName);

			FFormatNamedArguments Args;
			Args.Add(TEXT("Title"), BasicTitle);
			Args.Add(TEXT("SyncGroup"), SyncGroupName);

			if (TitleType == ENodeTitleType::FullTitle)
			{
				return FText::Format(LOCTEXT("SequenceNodeGroupWithSubtitleFull", "{Title}\nSync group {SyncGroup}"), Args);
			}
			else
			{
				return FText::Format(LOCTEXT("SequenceNodeGroupWithSubtitleList", "{Title} (Sync group {SyncGroup})"), Args);
			}
		}
	}
}

FText UAnimGraphNode_SequencePlayer::GetTitleGivenAssetInfo(const FText& AssetName, bool bKnownToBeAdditive)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	if (bKnownToBeAdditive)
	{
		return FText::Format(LOCTEXT("SequenceNodeTitleAdditive", "Play {AssetName} (additive)"), Args);
	}
	else
	{
		return FText::Format(LOCTEXT("SequenceNodeTitle", "Play {AssetName}"), Args);
	}
}

FText UAnimGraphNode_SequencePlayer::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Animation);
}

void UAnimGraphNode_SequencePlayer::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto LoadedAssetSetup = [](UEdGraphNode* NewNode, bool /*bIsTemplateNode*/, TWeakObjectPtr<UAnimSequence> SequencePtr)
	{
		UAnimGraphNode_SequencePlayer* SequencePlayerNode = CastChecked<UAnimGraphNode_SequencePlayer>(NewNode);
		SequencePlayerNode->Node.Sequence = SequencePtr.Get();
	};

	auto UnloadedAssetSetup = [](UEdGraphNode* NewNode, bool bIsTemplateNode, const FAssetData AssetData)
	{
		UAnimGraphNode_SequencePlayer* SequencePlayerNode = CastChecked<UAnimGraphNode_SequencePlayer>(NewNode);
		if (bIsTemplateNode)
		{
			if (const FString* SkeletonTag = AssetData.TagsAndValues.Find(TEXT("Skeleton")))
			{
				SequencePlayerNode->UnloadedSkeletonName = *SkeletonTag;
			}
		}
		else
		{
			UAnimSequence* Sequence = Cast<UAnimSequence>(AssetData.GetAsset());
			check(Sequence != nullptr);
			SequencePlayerNode->Node.Sequence = Sequence;
		}
	};	

	const UObject* QueryObject = ActionRegistrar.GetActionKeyFilter();
	if (QueryObject == nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		// define a filter to help in pulling UAnimSequence asset data from the registry
		FARFilter Filter;
		Filter.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
		Filter.bRecursiveClasses = true;
		// Find matching assets and add an entry for each one
		TArray<FAssetData> SequenceList;
		AssetRegistryModule.Get().GetAssets(Filter, /*out*/SequenceList);

		for (auto AssetIt = SequenceList.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;			

			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			if (Asset.IsAssetLoaded())
			{
				TWeakObjectPtr<UAnimSequence> AnimSequence = Cast<UAnimSequence>(Asset.GetAsset());
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(LoadedAssetSetup, AnimSequence);
				NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(AnimSequence->GetFName()), AnimSequence->IsValidAdditive());
			}
			else
			{
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(UnloadedAssetSetup, Asset);
				NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(Asset.AssetName), /*bKnownToBeAdditive =*/false);
			}
			ActionRegistrar.AddBlueprintAction(Asset, NodeSpawner);
		}
	}
	else if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(QueryObject))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());

		TWeakObjectPtr<UAnimSequence> SequencePtr = AnimSequence;
		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(LoadedAssetSetup, SequencePtr);
		NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(AnimSequence->GetFName()), AnimSequence->IsValidAdditive());

		ActionRegistrar.AddBlueprintAction(QueryObject, NodeSpawner);
	}
	else if (QueryObject == GetClass())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		// define a filter to help in pulling UAnimSequence asset data from the registry
		FARFilter Filter;
		Filter.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
		Filter.bRecursiveClasses = true;
		// Find matching assets and add an entry for each one
		TArray<FAssetData> SequenceList;
		AssetRegistryModule.Get().GetAssets(Filter, /*out*/SequenceList);

		for (auto AssetIt = SequenceList.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;
			if (Asset.IsAssetLoaded())
			{
				continue;
			}

			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(UnloadedAssetSetup, Asset);
			NodeSpawner->DefaultMenuSignature.MenuName = GetTitleGivenAssetInfo(FText::FromName(Asset.AssetName), /*bKnownToBeAdditive =*/false);
			ActionRegistrar.AddBlueprintAction(Asset, NodeSpawner);
		}
	}	
}

bool UAnimGraphNode_SequencePlayer::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	bool bIsFilteredOut = false;
	FBlueprintActionContext const& FilterContext = Filter.Context;

	for (UBlueprint* Blueprint : FilterContext.Blueprints)
	{
		if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
		{
			if(Node.Sequence)
			{
				if(Node.Sequence->GetSkeleton() != AnimBlueprint->TargetSkeleton)
				{
					// Sequence does not use the same skeleton as the Blueprint, cannot use
					bIsFilteredOut = true;
					break;
				}
			}
			else 
			{
				FAssetData SkeletonData(AnimBlueprint->TargetSkeleton);
				if(UnloadedSkeletonName != SkeletonData.GetExportTextName())
				{
					bIsFilteredOut = true;
					break;
				}
			}
		}
		else
		{
			// Not an animation Blueprint, cannot use
			bIsFilteredOut = true;
			break;
		}
	}
	return bIsFilteredOut;
}

void UAnimGraphNode_SequencePlayer::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog)
{
	if (Node.Sequence == NULL)
	{
		MessageLog.Error(TEXT("@@ references an unknown sequence"), this);
	}
	else
	{
		USkeleton* SeqSkeleton = Node.Sequence->GetSkeleton();
		if (SeqSkeleton&& // if anim sequence doesn't have skeleton, it might be due to anim sequence not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!SeqSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references sequence that uses different skeleton @@"), this, SeqSkeleton);
		}
	}
}

void UAnimGraphNode_SequencePlayer::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		// add an option to convert to single frame
		Context.MenuBuilder->BeginSection("AnimGraphNodeSequencePlayer", NSLOCTEXT("A3Nodes", "SequencePlayerHeading", "Sequence Player"));
		{
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
			Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ConvertToSeqEvaluator);
		}
		Context.MenuBuilder->EndSection();
	}
}

void UAnimGraphNode_SequencePlayer::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	Node.GroupIndex = AnimBlueprint->FindOrAddGroup(SyncGroup.GroupName);
	Node.GroupRole = SyncGroup.GroupRole;
}

void UAnimGraphNode_SequencePlayer::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& ComplexAnims, TArray<UAnimSequence*>& AnimationSequences) const
{
	if(Node.Sequence)
	{
		HandleAnimReferenceCollection(Node.Sequence, ComplexAnims, AnimationSequences);
	}
}

void UAnimGraphNode_SequencePlayer::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ComplexAnimsMap, const TMap<UAnimSequence*, UAnimSequence*>& AnimSequenceMap)
{
	HandleAnimReferenceReplacement(Node.Sequence, ComplexAnimsMap, AnimSequenceMap);
}


bool UAnimGraphNode_SequencePlayer::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_SequencePlayer::GetAnimationAsset() const 
{
	return Node.Sequence;
}

const TCHAR* UAnimGraphNode_SequencePlayer::GetTimePropertyName() const 
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_SequencePlayer::GetTimePropertyStruct() const 
{
	return FAnimNode_SequencePlayer::StaticStruct();
}

#undef LOCTEXT_NAMESPACE