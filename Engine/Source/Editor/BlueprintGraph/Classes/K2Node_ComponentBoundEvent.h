// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_Event.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_ComponentBoundEvent.generated.h"

class UDynamicBlueprintBinding;

UCLASS(MinimalAPI)
class UK2Node_ComponentBoundEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

	/** Delegate property name that this event is associated with */
	UPROPERTY()
	FName DelegatePropertyName;

	/** Delegate property's owner class that this event is associated with */
	UPROPERTY()
	UClass* DelegateOwnerClass;

	/** Name of property in Blueprint class that pointer to component we want to bind to */
	UPROPERTY()
	FName ComponentPropertyName;

	// Begin UObject interface
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void Serialize(FArchive& Ar) override;
	// End UObject interface

	// Begin UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetDocumentationLink() const override;
	virtual FString GetDocumentationExcerptName() const override;
	// End UEdGraphNode interface

	// Begin K2Node interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;
	// End K2Node interface

	virtual bool IsUsedByAuthorityOnlyDelegate() const override;

	/** Return the delegate property that this event is bound to */
	BLUEPRINTGRAPH_API UMulticastDelegateProperty* GetTargetDelegateProperty() const;

	BLUEPRINTGRAPH_API void InitializeComponentBoundEventParams(UObjectProperty const* InComponentProperty, const UMulticastDelegateProperty* InDelegateProperty);

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};

