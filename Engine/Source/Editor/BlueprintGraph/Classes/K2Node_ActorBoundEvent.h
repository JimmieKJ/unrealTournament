// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_Event.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_ActorBoundEvent.generated.h"

UCLASS(MinimalAPI)
class UK2Node_ActorBoundEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

	/** Delegate property name that this event is associated with */
	UPROPERTY()
	FName DelegatePropertyName;

	/** Delegate property's owner class that this event is associated with */
	UPROPERTY()
	UClass* DelegateOwnerClass;

	/** The event that this event is bound to */
	UPROPERTY()
	class AActor* EventOwner;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	// Begin UEdGraphNode interface
	virtual void ReconstructNode() override;
	virtual void DestroyNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetDocumentationLink() const override;
	virtual FString GetDocumentationExcerptName() const override;
	// End UEdGraphNode interface

	// Begin K2Node interface
	virtual AActor* GetReferencedLevelActor() const override;
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual FNodeHandlingFunctor* CreateNodeHandler(FKismetCompilerContext& CompilerContext) const override;
	// End K2Node interface

	virtual bool IsUsedByAuthorityOnlyDelegate() const override;

	/**
	 * Initialized the members of the node, given the specified owner and delegate property.  This will fill out all the required members for the event, such as CustomFunctionName
	 *
	 * @param InEventOwner			The target for this bound event
	 * @param InDelegateProperty	The multicast delegate property associated with the event, which will have a delegate added to it in the level script actor, matching its signature
	 */
	BLUEPRINTGRAPH_API void InitializeActorBoundEventParams(AActor* InEventOwner, const UMulticastDelegateProperty* InDelegateProperty);

	/** Return the delegate property that this event is bound to */
	BLUEPRINTGRAPH_API UMulticastDelegateProperty* GetTargetDelegateProperty();
	/** Const version of GetTargetDelegateProperty that does not search redirect table */
	BLUEPRINTGRAPH_API UMulticastDelegateProperty* GetTargetDelegatePropertyConst() const;

	/** Returns the delegate that this event is bound in */
	BLUEPRINTGRAPH_API FMulticastScriptDelegate* GetTargetDelegate();

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};

