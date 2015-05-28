// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "BlueprintEventNodeSpawner.h"
#include "EdGraphSchema_K2.h" // for GetFriendlySignatureName()
#include "BlueprintNodeTemplateCache.h" // for IsTemplateOuter()

#define LOCTEXT_NAMESPACE "BlueprintEventNodeSpawner"

/*******************************************************************************
 * Static UBlueprintEventNodeSpawner Helpers
 ******************************************************************************/

namespace UBlueprintEventNodeSpawnerImpl
{
	/**
	 * Helper function for scanning a blueprint for a certain, custom named,
	 * event.
	 * 
	 * @param  Blueprint	The blueprint you want to look through
	 * @param  CustomName	The event name you want to check for.
	 * @return Null if no event was found, otherwise a pointer to the named event.
	 */
	static UK2Node_Event* FindCustomEventNode(UBlueprint* Blueprint, FName const CustomName);

	/**
	 * Helper function for removing all disabled nodes connected to a 
	 * disabled source node
	 *
	 * @param InNode			The node to start with
	 * @param InParentGraph		The graph to remove the nodes from
	 */
	static void RemoveAllDisabledNodes(UEdGraphNode* InNode, UEdGraph* InParentGraph);
}

//------------------------------------------------------------------------------
UK2Node_Event* UBlueprintEventNodeSpawnerImpl::FindCustomEventNode(UBlueprint* Blueprint, FName const CustomName)
{
	UK2Node_Event* FoundNode = nullptr;

	if (CustomName != NAME_None)
	{
		TArray<UK2Node_Event*> AllEvents;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, AllEvents);

		for (UK2Node_Event* EventNode : AllEvents)
		{
			if (EventNode->CustomFunctionName == CustomName)
			{
				FoundNode = EventNode;
				break;
			}
		}
	}	
	return FoundNode;
}

//------------------------------------------------------------------------------
static void UBlueprintEventNodeSpawnerImpl::RemoveAllDisabledNodes(UEdGraphNode* InNode, UEdGraph* InParentGraph)
{
	if(InNode && !InNode->bIsNodeEnabled)
	{
		// Go through all pin connections and consume any disabled nodes so we do not leave garbage.
		for (UEdGraphPin* Pin : InNode->Pins)
		{
			TArray<UEdGraphPin*> LinkedToCopy = Pin->LinkedTo;
			for (UEdGraphPin* OtherPin : LinkedToCopy)
			{
				// Break the pin link back
				OtherPin->BreakLinkTo(Pin);
				RemoveAllDisabledNodes(OtherPin->GetOwningNode(), InParentGraph);
			}
		}

		InNode->BreakAllNodeLinks();
		InParentGraph->RemoveNode(InNode);
	}
};

/*******************************************************************************
 * UBlueprintEventNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintEventNodeSpawner* UBlueprintEventNodeSpawner::Create(UFunction const* const EventFunc, UObject* Outer/* = nullptr*/)
{
	check(EventFunc != nullptr);

	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	UBlueprintEventNodeSpawner* NodeSpawner = NewObject<UBlueprintEventNodeSpawner>(Outer);
	NodeSpawner->EventFunc = EventFunc;
	NodeSpawner->NodeClass = UK2Node_Event::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	FText const FuncName = UEdGraphSchema_K2::GetFriendlySignatureName(EventFunc);
	MenuSignature.MenuName = FText::Format(LOCTEXT("EventWithSignatureName", "Event {0}"), FuncName);
	FString const FuncCategory = UK2Node_CallFunction::GetDefaultCategoryForFunction(EventFunc, TEXT(""));
	MenuSignature.Category = FText::FromString(LOCTEXT("AddEventCategory", "Add Event").ToString() + TEXT("|") + FuncCategory);
	//MenuSignature.Tooltip, will be pulled from the node template
	MenuSignature.Keywords = UK2Node_CallFunction::GetKeywordsForFunction(EventFunc);
	if (MenuSignature.Keywords.IsEmpty())
	{
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}
	MenuSignature.IconName = TEXT("GraphEditor.Event_16x");

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintEventNodeSpawner* UBlueprintEventNodeSpawner::Create(TSubclassOf<UK2Node_Event> NodeClass, FName CustomEventName, UObject* Outer/* = nullptr*/)
{
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	UBlueprintEventNodeSpawner* NodeSpawner = NewObject<UBlueprintEventNodeSpawner>(Outer);
	NodeSpawner->NodeClass       = NodeClass;
	NodeSpawner->CustomEventName = CustomEventName;

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	if (CustomEventName.IsNone())
	{
		MenuSignature.MenuName = LOCTEXT("AddCustomEvent", "Add Custom Event...");
		MenuSignature.IconName = TEXT("GraphEditor.CustomEvent_16x");
	}
	else
	{
		FText const EventName = FText::FromName(CustomEventName);
		MenuSignature.MenuName = FText::Format(LOCTEXT("EventWithSignatureName", "Event {0}"), EventName);
		MenuSignature.IconName = TEXT("GraphEditor.Event_16x");
	}
	//MenuSignature.Category, will be pulled from the node template
	//MenuSignature.Tooltip,  will be pulled from the node template 
	//MenuSignature.Keywords, will be pulled from the node template

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintEventNodeSpawner::UBlueprintEventNodeSpawner(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, EventFunc(nullptr)
{
}

//------------------------------------------------------------------------------
FBlueprintNodeSignature UBlueprintEventNodeSpawner::GetSpawnerSignature() const
{
	FBlueprintNodeSignature SpawnerSignature(NodeClass);
	if (IsForCustomEvent() && !CustomEventName.IsNone())
	{
		static const FName CustomSignatureKey(TEXT("CustomEvent"));
		SpawnerSignature.AddNamedValue(CustomSignatureKey, CustomEventName.ToString());
	}
	else
	{
		SpawnerSignature.AddSubObject(EventFunc);
	}
	return SpawnerSignature;
}

//------------------------------------------------------------------------------
UEdGraphNode* UBlueprintEventNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	check(ParentGraph != nullptr);
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);

	UK2Node_Event* EventNode = nullptr;
	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	if (!bIsTemplateNode)
	{
		// look to see if a node for this event already exists (only one node is
		// allowed per event, per blueprint)
		UK2Node_Event const* PreExistingNode = FindPreExistingEvent(Blueprint, Bindings);
		// @TODO: casting away the const is bad form!
		EventNode = const_cast<UK2Node_Event*>(PreExistingNode);
	}

	bool const bIsCustomEvent = IsForCustomEvent();
	check(bIsCustomEvent || (EventFunc != nullptr));

	FName EventName = CustomEventName;
	if (!bIsCustomEvent)
	{
		EventName  = EventFunc->GetFName();	
	}

	// This Event node might already be present in the Blueprint in a disabled state, 
	// remove it and allow the user to successfully place the node where they want it.
	if(EventNode && !EventNode->bIsNodeEnabled)
	{
		UBlueprintEventNodeSpawnerImpl::RemoveAllDisabledNodes(EventNode, ParentGraph);
		EventNode = nullptr;
	}

	// if there is no existing node, then we can happily spawn one into the graph
	if (EventNode == nullptr)
	{
		auto PostSpawnLambda = [](UEdGraphNode* NewNode, bool bInIsTemplateNode, UFunction const* InEventFunc, FName InEventName, FCustomizeNodeDelegate UserDelegate)
		{
			UK2Node_Event* K2EventNode = CastChecked<UK2Node_Event>(NewNode);
			if (InEventFunc != nullptr)
			{
				K2EventNode->EventReference.SetFromField<UFunction>(InEventFunc, false);
				K2EventNode->bOverrideFunction   = true;
			}
			else if (!bInIsTemplateNode)
			{
				K2EventNode->CustomFunctionName = InEventName;
			}

			UserDelegate.ExecuteIfBound(NewNode, bInIsTemplateNode);
		};

		FCustomizeNodeDelegate PostSpawnDelegate = FCustomizeNodeDelegate::CreateStatic(PostSpawnLambda, EventFunc, EventName, CustomizeNodeDelegate);
		EventNode = Super::SpawnNode<UK2Node_Event>(NodeClass, ParentGraph, Bindings, Location, PostSpawnDelegate);
	}
	// else, a node for this event already exists, and we should return that 
	// (the FBlueprintActionMenuItem should detect this and focus in on it).

	return EventNode;
}

//------------------------------------------------------------------------------
UFunction const* UBlueprintEventNodeSpawner::GetEventFunction() const
{
	return EventFunc;
}

//------------------------------------------------------------------------------
UK2Node_Event const* UBlueprintEventNodeSpawner::FindPreExistingEvent(UBlueprint* Blueprint, FBindingSet const& /*Bindings*/) const
{
	UK2Node_Event* PreExistingNode = nullptr;

	check(Blueprint != nullptr);
	if (IsForCustomEvent())
	{
		PreExistingNode = UBlueprintEventNodeSpawnerImpl::FindCustomEventNode(Blueprint, CustomEventName);
	}
	else
	{
		check(EventFunc != nullptr);
		UClass* ClassOwner = EventFunc->GetOwnerClass()->GetAuthoritativeClass();

		PreExistingNode = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, ClassOwner, EventFunc->GetFName());
	}

	return PreExistingNode;
}

//------------------------------------------------------------------------------
bool UBlueprintEventNodeSpawner::IsForCustomEvent() const
{
	return (EventFunc == nullptr);
}

#undef LOCTEXT_NAMESPACE
