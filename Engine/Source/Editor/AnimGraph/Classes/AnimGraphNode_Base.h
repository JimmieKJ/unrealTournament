// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "AnimGraphNode_Base.generated.h"

// Forward declarations

struct FEdGraphSchemaAction_K2NewNode;
struct FPropertyChangedEvent;
class FCompilerResultsLog;
class UAnimGraphNode_Base;
class USkeleton;
class UAnimBlueprintGeneratedClass;
class IDetailLayoutBuilder;
class FAnimBlueprintCompiler;
class FAnimGraphNodeDetails;

// 
struct FPoseLinkMappingRecord
{
public:
	static FPoseLinkMappingRecord MakeFromArrayEntry(UAnimGraphNode_Base* LinkingNode, UAnimGraphNode_Base* LinkedNode, UArrayProperty* ArrayProperty, int32 ArrayIndex)
	{
		checkSlow(CastChecked<UStructProperty>(ArrayProperty->Inner)->Struct->IsChildOf(FPoseLinkBase::StaticStruct()));

		FPoseLinkMappingRecord Result;
		Result.LinkingNode = LinkingNode;
		Result.LinkedNode = LinkedNode;
		Result.ChildProperty = ArrayProperty;
		Result.ChildPropertyIndex = ArrayIndex;

		return Result;
	}

	static FPoseLinkMappingRecord MakeFromMember(UAnimGraphNode_Base* LinkingNode, UAnimGraphNode_Base* LinkedNode, UStructProperty* MemberProperty)
	{
		checkSlow(MemberProperty->Struct->IsChildOf(FPoseLinkBase::StaticStruct()));

		FPoseLinkMappingRecord Result;
		Result.LinkingNode = LinkingNode;
		Result.LinkedNode = LinkedNode;
		Result.ChildProperty = MemberProperty;
		Result.ChildPropertyIndex = INDEX_NONE;

		return Result;
	}

	static FPoseLinkMappingRecord MakeInvalid()
	{
		FPoseLinkMappingRecord Result;
		return Result;
	}

	bool IsValid() const
	{
		return LinkedNode != nullptr;
	}

	UAnimGraphNode_Base* GetLinkedNode() const
	{
		return LinkedNode;
	}

	UAnimGraphNode_Base* GetLinkingNode() const
	{
		return LinkingNode;
	}

	ANIMGRAPH_API void PatchLinkIndex(uint8* DestinationPtr, int32 LinkID, int32 SourceLinkID) const;
protected:
	FPoseLinkMappingRecord()
		: LinkedNode(nullptr)
		, LinkingNode(nullptr)
		, ChildProperty(nullptr)
		, ChildPropertyIndex(INDEX_NONE)
	{
	}

protected:
	// Linked node for this pose link, can be nullptr
	UAnimGraphNode_Base* LinkedNode;

	// Linking node for this pose link, can be nullptr
	UAnimGraphNode_Base* LinkingNode;

	// Will either be an array property containing FPoseLinkBase derived structs, indexed by ChildPropertyIndex, or a FPoseLinkBase derived struct property 
	UProperty* ChildProperty;

	// Index when ChildProperty is an array
	int32 ChildPropertyIndex;
};

UENUM()
enum class EBlueprintUsage : uint8
{
	NoProperties,
	DoesNotUseBlueprint,
	UsesBlueprint
};

/**
  * This is the base class for any animation graph nodes that generate or consume an animation pose in
  * the animation blend graph.
  *
  * Any concrete implementations will be paired with a runtime graph node derived from FAnimNode_Base
  */
UCLASS(Abstract)
class ANIMGRAPH_API UAnimGraphNode_Base : public UK2Node
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PinOptions, EditFixedSize)
	TArray<FOptionalPinFromProperty> ShowPinForProperties;

	UPROPERTY(Transient)
	EBlueprintUsage BlueprintUsage;

	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FString GetDocumentationLink() const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual bool ShowPaletteIconOnNode() const override{ return false; }
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual bool CanPlaceBreakpoints() const override { return false; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const override;
	virtual void GetNodeAttributes(TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;

	// By default return any animation assets we have
	virtual UObject* GetJumpTargetForDoubleClick() const override { return GetAnimationAsset(); }
	// End of UK2Node interface

	// UAnimGraphNode_Base interface

	// Gets the menu category this node belongs in
	virtual FString GetNodeCategory() const;

	// Is this node a sink that has no pose outputs?
	virtual bool IsSinkNode() const { return false; }

	// Create any output pins necessary for this node
	virtual void CreateOutputPins();

	// customize pin data based on the input
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const {}

	// Gives each visual node a chance to do final validation before it's node is harvested for use at runtime
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) {}

	// Gives each visual node a chance to validate that they are still valid in the context of the compiled class, giving a last shot at error or warning generation after primary compilation is finished
	virtual void ValidateAnimNodePostCompile(FCompilerResultsLog& MessageLog, UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex) {}

	// Gives each visual node a chance to update the node template before it is inserted in the compiled class
	virtual void BakeDataDuringCompilation(FCompilerResultsLog& MessageLog) {}

	// preload asset required for this node in this function
	virtual void PreloadRequiredAssets() {}

	// Give the node a chance to change the display name of a pin
	virtual void PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const;

	/** Get the animation blueprint to which this node belongs */
	UAnimBlueprint* GetAnimBlueprint() const { return CastChecked<UAnimBlueprint>(GetBlueprint()); }

	// Populate the supplied arrays with the currently reffered to animation assets 
	virtual void GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimAssets) const {}

	// Replace references to animations that exist in the supplied maps 	
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap) {};
	
	// Helper function for GetAllAnimationSequencesReferred
	void HandleAnimReferenceCollection(UAnimationAsset* AnimAsset, TArray<UAnimationAsset*>& AnimationAssets) const;

	// Helper function for ReplaceReferredAnimations	
	template<class AssetType>
	void HandleAnimReferenceReplacement(AssetType*& OriginalAsset, const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap);

	// BEGIN Interface to support transition getter
	// if you return true for DoesSupportExposeTimeForTransitionGetter
	// you should implement all below functions
	virtual bool DoesSupportTimeForTransitionGetter() const { return false; }
	virtual UAnimationAsset* GetAnimationAsset() const { return nullptr; }
	virtual const TCHAR* GetTimePropertyName() const { return nullptr; }
	virtual UScriptStruct* GetTimePropertyStruct() const { return nullptr; }
	// END Interface to support transition getter

	// can customize details tab 
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder){ }

	template<typename NodeType>
	NodeType* GetActiveInstanceNode(UObject* AnimInstanceObject) const
	{
		if(!AnimInstanceObject)
		{
			return nullptr;
		}

		if(UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstanceObject->GetClass()))
		{
			return AnimClass->GetPropertyInstance<NodeType>(AnimInstanceObject, NodeGuid);
		}

		return nullptr;
	}

protected:
	friend FAnimBlueprintCompiler;
	friend FAnimGraphNodeDetails;

	// Gets the animation FNode type represented by this ed graph node
	UScriptStruct* GetFNodeType() const;

	// Gets the animation FNode property represented by this ed graph node
	UStructProperty* GetFNodeProperty() const;

	// This will be called when a pose link is found, and can be called with PoseProperty being either of:
	//  - an array property (ArrayIndex >= 0)
	//  - a single pose property (ArrayIndex == INDEX_NONE)
	virtual void CreatePinsForPoseLink(UProperty* PoseProperty, int32 ArrayIndex);

	//
	virtual FPoseLinkMappingRecord GetLinkIDLocation(const UScriptStruct* NodeType, UEdGraphPin* InputLinkPin);

	//
	virtual void GetPinAssociatedProperty(const UScriptStruct* NodeType, UEdGraphPin* InputPin, UProperty*& OutProperty, int32& OutIndex);

	// Allocates or reallocates pins
	void InternalPinCreation(TArray<UEdGraphPin*>* OldPins);
};

template<class AssetType>
void UAnimGraphNode_Base::HandleAnimReferenceReplacement(AssetType*& OriginalAsset, const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	AssetType* CacheOriginalAsset = OriginalAsset;
	OriginalAsset = nullptr;

	if (UAnimationAsset* const* ReplacementAsset = AnimAssetReplacementMap.Find(CacheOriginalAsset))
	{
		OriginalAsset = Cast<AssetType>(*ReplacementAsset);
	}
}
