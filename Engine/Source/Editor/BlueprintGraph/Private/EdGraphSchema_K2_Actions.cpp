// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "ComponentAssetBroker.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Editor/UnrealEd/Public/EdGraphUtilities.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_Timeline.h"
#include "K2Node_Literal.h"
#include "K2Node_VariableGet.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CustomEvent.h"


#define SNAP_GRID (16) // @todo ensure this is the same as SNodePanel::GetSnapGridSize()


namespace 
{
	// Maximum distance a drag can be off a node edge to require 'push off' from node
	const int32 NodeDistance = 60;

	// The amount to offset a literal reference (to an actor) from the function node it is being connected to
	const float FunctionNodeLiteralReferencesXOffset = 224.0f;

	// The height of a literal reference node
	const float NodeLiteralHeight = 48.0f;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2ViewNode

UEdGraphNode* FEdGraphSchemaAction_K2NewNode::CreateNode(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, class UK2Node* NodeTemplate, bool bSelectNewNode/* = true*/)
{
	// Smart pointer that handles fixup after potential node reconstruction
	FWeakGraphPinPtr FromPinPtr = FromPin;

	// Duplicate template node to create new node
	UEdGraphNode* ResultNode = DuplicateObject<UK2Node>(NodeTemplate, ParentGraph);
	ResultNode->SetFlags(RF_Transactional);

	ParentGraph->AddNode(ResultNode, true, bSelectNewNode);

	ResultNode->CreateNewGuid();
	ResultNode->PostPlacedNewNode();
	ResultNode->AllocateDefaultPins();

	// For input pins, new node will generally overlap node being dragged off
	// Work out if we want to visually push away from connected node
	int32 XLocation = Location.X;
	if (FromPinPtr.IsValid() && FromPinPtr->Direction == EGPD_Input)
	{
		UEdGraphNode* PinNode = FromPinPtr->GetOwningNode();
		const float XDelta = FMath::Abs(PinNode->NodePosX - Location.X);

		if (XDelta < NodeDistance)
		{
			// Set location to edge of current node minus the max move distance
			// to force node to push off from connect node enough to give selection handle
			XLocation = PinNode->NodePosX - NodeDistance;
		}
	}
	ResultNode->NodePosX = XLocation;
	ResultNode->NodePosY = Location.Y;
	ResultNode->SnapToGrid(SNAP_GRID);

	// make sure to auto-wire after we position the new node (in case the 
	// auto-wire creates a conversion node to put between them)
	ResultNode->AutowireNewNode(FromPinPtr);

	// Update Analytics for the new nodes
	FBlueprintEditorUtils::AnalyticsTrackNewNode( ResultNode );
	// NOTE: At this point the node may have been reconstructed, depending on node type!

	return ResultNode;
}

UEdGraphNode* FEdGraphSchemaAction_K2NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "K2_AddNode", "Add Node") );
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		ResultNode = CreateNode(ParentGraph, FromPin, Location, NodeTemplate, bSelectNewNode);

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);

		// See if we need to recompile skeleton after adding this node, or just mark dirty
		UK2Node* K2Node = Cast<UK2Node>(ResultNode);
		if(K2Node != NULL && K2Node->NodeCausesStructuralBlueprintChange())
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		else
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}

		// Clear any error messages resulting from placing a node.  They'll be flagged on the next compile
		K2Node->ErrorMsg.Empty();
		K2Node->bHasCompilerMessage = false;

		if ( bGotoNode && ResultNode )
		{
			// Select existing node
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ResultNode);
		}
	}

	return ResultNode;
}

UEdGraphNode* FEdGraphSchemaAction_K2NewNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode/* = true*/) 
{
	UEdGraphNode* ResultNode = NULL;

	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph, FromPins[0], Location, bSelectNewNode);

		// Try autowiring the rest of the pins
		for (int32 Index = 1; Index < FromPins.Num(); ++Index)
		{
			ResultNode->AutowireNewNode(FromPins[Index]);
		}
	}
	else
	{
		ResultNode = PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
	}

	return ResultNode;
}

void FEdGraphSchemaAction_K2NewNode::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( NodeTemplate );
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2ViewNode

UEdGraphNode* FEdGraphSchemaAction_K2ViewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	// If the node is valid, select it
	if (NodePtr)
	{
		// Select existing node
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NodePtr);
	}

	return NULL;
}

UEdGraphNode* FEdGraphSchemaAction_K2ViewNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode/* = true*/) 
{
	PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
	return NULL;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AssignDelegate

UEdGraphNode* FEdGraphSchemaAction_K2AssignDelegate::AssignDelegate(class UK2Node* NodeTemplate, class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UK2Node_AddDelegate* BindNode = NULL;
	if (UK2Node_AddDelegate* AddDelegateTemplate = Cast<UK2Node_AddDelegate>(NodeTemplate))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "K2_AddNode", "Add Node") );
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		BindNode = Cast<UK2Node_AddDelegate>(CreateNode(ParentGraph, FromPin, Location, NodeTemplate, bSelectNewNode));
		UMulticastDelegateProperty* DelegateProperty = BindNode ? Cast<UMulticastDelegateProperty>(BindNode->GetProperty()) : NULL;
		if(DelegateProperty)
		{
			const FString FunctionName = FString::Printf(TEXT("%s_Event"), *DelegateProperty->GetName());
			UK2Node_CustomEvent* EventNode = UK2Node_CustomEvent::CreateFromFunction(
				FVector2D(Location.X - 150, Location.Y + 150), 
				ParentGraph, FunctionName, DelegateProperty->SignatureFunction, bSelectNewNode);
			if(EventNode)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
				UEdGraphPin* OutDelegatePin = EventNode->FindPinChecked(UK2Node_CustomEvent::DelegateOutputName);
				UEdGraphPin* InDelegatePin = BindNode->GetDelegatePin();
				K2Schema->TryCreateConnection(OutDelegatePin, InDelegatePin);
			}
		}

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	return BindNode;
}

UEdGraphNode* FEdGraphSchemaAction_K2AssignDelegate::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	return AssignDelegate(NodeTemplate, ParentGraph, FromPin, Location, bSelectNewNode);
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_EventFromFunction

UEdGraphNode* FEdGraphSchemaAction_EventFromFunction::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	UK2Node_CustomEvent* EventNode = NULL;
	if (SignatureFunction)
	{
		if (FromPin)
		{
			// Make sure, that function is latest, so the names of parameters are proper.
			UK2Node_BaseMCDelegate* MCDelegateNode = Cast<UK2Node_BaseMCDelegate>(FromPin->GetOwningNode());
			UEdGraphPin* InputDelegatePin = MCDelegateNode ? MCDelegateNode->GetDelegatePin() : NULL;
			UFunction* OriginalFunction = MCDelegateNode ? MCDelegateNode->GetDelegateSignature() : NULL;
			if (OriginalFunction && 
				(OriginalFunction != SignatureFunction) && 
				(FromPin == InputDelegatePin) &&
				SignatureFunction->IsSignatureCompatibleWith(OriginalFunction))
			{
				SignatureFunction = OriginalFunction;
			}
		}

		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "K2_AddNode", "Add Node") );
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		EventNode = UK2Node_CustomEvent::CreateFromFunction(Location, ParentGraph, SignatureFunction->GetName() + TEXT("_Event"), SignatureFunction, bSelectNewNode);
		EventNode->AutowireNewNode(FromPin);

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	return EventNode;
}

UEdGraphNode* FEdGraphSchemaAction_EventFromFunction::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphNode* ResultNode = NULL;

	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph, FromPins[0], Location, bSelectNewNode);

		// Try autowiring the rest of the pins
		for (int32 Index = 1; Index < FromPins.Num(); ++Index)
		{
			ResultNode->AutowireNewNode(FromPins[Index]);
		}
	}
	else
	{
		ResultNode = PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
	}

	return ResultNode;
}

void FEdGraphSchemaAction_EventFromFunction::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject(SignatureFunction);
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddComponent

UEdGraphNode* FEdGraphSchemaAction_K2AddComponent::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	if (ComponentClass == NULL)
	{
		return NULL;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
	UEdGraphNode* NewNode = FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);
	if ((NewNode != NULL) && (Blueprint != NULL))
	{
		UK2Node_AddComponent* AddCompNode = CastChecked<UK2Node_AddComponent>(NewNode);

		ensure(NULL != Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass));
		// Then create a new template object, and add to array in
		UActorComponent* NewTemplate = NewObject<UActorComponent>(Blueprint->GeneratedClass, ComponentClass, NAME_None, RF_ArchetypeObject | RF_Public);
		Blueprint->ComponentTemplates.Add(NewTemplate);

		// Set the name of the template as the default for the TemplateName param
		UEdGraphPin* TemplateNamePin = AddCompNode->GetTemplateNamePinChecked();
		if (TemplateNamePin)
		{
			TemplateNamePin->DefaultValue = NewTemplate->GetName();
		}

		// Set the return type to be the type of the template
		UEdGraphPin* ReturnPin = AddCompNode->GetReturnValuePin();
		if (ReturnPin)
		{
			ReturnPin->PinType.PinSubCategoryObject = *ComponentClass;
		}

		// Set the asset
		if(ComponentAsset != NULL)
		{
			FComponentAssetBrokerage::AssignAssetToComponent(NewTemplate, ComponentAsset);
		}

		AddCompNode->ReconstructNode();
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	return NewNode;
}

void FEdGraphSchemaAction_K2AddComponent::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction_K2NewNode::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( ComponentAsset );
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddTimeline

UEdGraphNode* FEdGraphSchemaAction_K2AddTimeline::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "K2_AddTimeline", "Add Timeline") );
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
	UEdGraphNode* NewNode = FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);

	// Set the name to be a unique timeline name, so the generated node will have a default name that is already validated
	UK2Node_Timeline* TimelineNode = CastChecked<UK2Node_Timeline>(NewNode);
	TimelineNode->TimelineName = FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint);

	if(Blueprint != NULL)
	{
		if (FBlueprintEditorUtils::AddNewTimeline(Blueprint, TimelineNode->TimelineName) != NULL)
		{
			// Clear off any existing error message now the timeline has been added
			TimelineNode->ErrorMsg.Empty();
			TimelineNode->bHasCompilerMessage = false;
		}
	}

	return NewNode;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddEvent

UEdGraphNode* FEdGraphSchemaAction_K2AddEvent::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphNode* NewNode = NULL;
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "K2_Event", "Add Event") );	

	UBlueprint const* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);

	UK2Node_Event const* ExistingEvent = NULL;
	if (EventHasAlreadyBeenPlaced(Blueprint, &ExistingEvent))
	{
		check(ExistingEvent != NULL);
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ExistingEvent);
	}
	else
	{
		NewNode = FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);
	}

	return NewNode;
}

bool FEdGraphSchemaAction_K2AddEvent::EventHasAlreadyBeenPlaced(UBlueprint const* Blueprint, UK2Node_Event const** FoundEventOut /*= NULL*/) const
{
	UK2Node_Event* ExistingEvent = NULL;

	if (Blueprint != NULL)
	{
		UK2Node_Event const* EventTemplate = Cast<UK2Node_Event const>(NodeTemplate);
		ExistingEvent = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, EventTemplate->EventReference.GetMemberParentClass(EventTemplate->GetBlueprintClassFromNode()), EventTemplate->EventReference.GetMemberName());
	}

	if (FoundEventOut != NULL)
	{
		*FoundEventOut = ExistingEvent;
	}

	return (ExistingEvent != NULL);
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddCustomEvent

UEdGraphNode* FEdGraphSchemaAction_K2AddCustomEvent::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "K2_CustomEvent", "Add Custom Event") );
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);

	UEdGraphNode* NewNode = FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);

	// Set the name for the template to be a unique custom event name, so the generated node will have a default name that is already validated
	UK2Node_CustomEvent* CustomEventNode = CastChecked<UK2Node_CustomEvent>(NewNode);
	CustomEventNode->CustomFunctionName = FBlueprintEditorUtils::FindUniqueCustomEventName(Blueprint);

	return NewNode;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddCallOnActor

UEdGraphNode* FEdGraphSchemaAction_K2AddCallOnActor::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Snap the node placement location to the grid, ensures calculations later match up better
	FVector2D LocalLocation;
	LocalLocation.X = FMath::GridSnap( Location.X, SNAP_GRID );
	LocalLocation.Y = FMath::GridSnap( Location.Y, SNAP_GRID );

	// First use the base functionality to spawn the 'call function' node
	UEdGraphNode* CallNode = FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, LocalLocation);
	const float FunctionNodeHeightUnsnapped = UEdGraphSchema_K2::EstimateNodeHeight( CallNode );

	// this is the guesstimate of the function node's height, snapped to grid units
	const float FunctionNodeHeight = FMath::GridSnap( FunctionNodeHeightUnsnapped, SNAP_GRID );
	// this is roughly the middle of the function node height
	const float FunctionNodeMidY = LocalLocation.Y + FunctionNodeHeight * 0.5f;
	// this is the offset up from the mid point at which we start placing nodes 
	const float StartYOffset = (float((LevelActors.Num() > 0) ? LevelActors.Num()-1 : 0) * -NodeLiteralHeight) * 0.5f;
	// The Y location we start placing nodes from
	const float ReferencedNodesPlacementYLocation = FunctionNodeMidY + StartYOffset;

	// Now we need to create the actor literal to wire up
	for ( int32 ActorIndex = 0; ActorIndex < LevelActors.Num(); ActorIndex++ )
	{
		AActor* LevelActor = LevelActors[ActorIndex];

		if(LevelActor != NULL)
		{
			UK2Node_Literal* LiteralNode =  NewObject<UK2Node_Literal>(ParentGraph);
			ParentGraph->AddNode(LiteralNode, false, bSelectNewNode);
			LiteralNode->SetFlags(RF_Transactional);

			LiteralNode->SetObjectRef(LevelActor);
			LiteralNode->AllocateDefaultPins();
			LiteralNode->NodePosX = LocalLocation.X - FunctionNodeLiteralReferencesXOffset;

			// this is the current offset down from the Y start location to place the next node at
			float CurrentNodeOffset = NodeLiteralHeight * float(ActorIndex);
			LiteralNode->NodePosY = ReferencedNodesPlacementYLocation + CurrentNodeOffset;

			LiteralNode->SnapToGrid(SNAP_GRID);

			// Connect the literal out to the self of the call
			UEdGraphPin* LiteralOutput = LiteralNode->GetValuePin();
			UEdGraphPin* CallSelfInput = CallNode->FindPin(K2Schema->PN_Self);
			if(LiteralOutput != NULL && CallSelfInput != NULL)
			{
				LiteralOutput->MakeLinkTo(CallSelfInput);
			}
		}
	}

	return CallNode;
}

void FEdGraphSchemaAction_K2AddCallOnActor::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction_K2NewNode::AddReferencedObjects( Collector );

	for ( int32 ActorIndex = 0; ActorIndex < LevelActors.Num(); ActorIndex++ )
	{
		if ( LevelActors[ActorIndex] != NULL )
		{
			Collector.AddReferencedObject( LevelActors[ActorIndex] );
		}
	}
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddCallOnVariable

UEdGraphNode* FEdGraphSchemaAction_K2AddCallOnVariable::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Snap the node placement location to the grid, ensures calculations later match up better
	FVector2D LocalLocation;
	LocalLocation.X = FMath::GridSnap( Location.X, SNAP_GRID );
	LocalLocation.Y = FMath::GridSnap( Location.Y, SNAP_GRID );

	// First use the base functionality to spawn the 'call function' node
	FVector2D TempLocation = LocalLocation;
	UEdGraphNode* CallNode = FEdGraphSchemaAction_K2NewNode::PerformAction(ParentGraph, FromPin, TempLocation, bSelectNewNode);

	// this is the guesstimate of the function node's height, snapped to grid units
	const float FunctionNodeHeight = FMath::GridSnap( TempLocation.Y - LocalLocation.Y, SNAP_GRID );
	// this is roughly the middle of the function node height
	const float FunctionNodeMidY = LocalLocation.Y + FunctionNodeHeight * 0.5f;
	// this is the offset up from the mid point at which we start placing nodes 
	const float StartYOffset = -NodeLiteralHeight * 0.5f;
	// The Y location we start placing nodes from
	const float ReferencedNodesPlacementYLocation = FunctionNodeMidY + StartYOffset;

	// Now we need to create the variable literal to wire up
	if(VariableName != NAME_None)
	{
		UK2Node_VariableGet* GetVarNode =  NewObject<UK2Node_VariableGet>(ParentGraph);
		ParentGraph->AddNode(GetVarNode, false, bSelectNewNode);
		GetVarNode->SetFlags(RF_Transactional);

		GetVarNode->VariableReference.SetSelfMember(VariableName);
		GetVarNode->AllocateDefaultPins();
		GetVarNode->NodePosX = LocalLocation.X - FunctionNodeLiteralReferencesXOffset;
		GetVarNode->NodePosY = ReferencedNodesPlacementYLocation;

		GetVarNode->SnapToGrid(SNAP_GRID);

		// Connect the literal out to the self of the call
		UEdGraphPin* LiteralOutput = GetVarNode->GetValuePin();
		UEdGraphPin* CallSelfInput = CallNode->FindPin(K2Schema->PN_Self);
		if(LiteralOutput != NULL && CallSelfInput != NULL)
		{
			LiteralOutput->MakeLinkTo(CallSelfInput);
		}
	}

	return CallNode;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddComment

UEdGraphNode* FEdGraphSchemaAction_K2AddComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	// Add menu item for creating comment boxes
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	FVector2D SpawnLocation = Location;

	FSlateRect Bounds;
	if ((Blueprint != NULL) && FKismetEditorUtilities::GetBoundsForSelectedNodes(Blueprint, Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	UEdGraphNode* NewNode = FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation, bSelectNewNode);

	// Update Analytics for these nodes
	FBlueprintEditorUtils::AnalyticsTrackNewNode( NewNode );

	// Mark Blueprint as structurally modified since
	// UK2Node_Comment::NodeCausesStructuralBlueprintChange used to return true
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	return NewNode;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2AddDocumentation

UEdGraphNode* FEdGraphSchemaAction_K2AddDocumentation::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	// Add menu item for creating document nodes
	UEdGraphNode_Documentation* DocumentTemplate = NewObject<UEdGraphNode_Documentation>();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	FVector2D SpawnLocation = Location;

	FSlateRect Bounds;
	if ((Blueprint != NULL) && FKismetEditorUtilities::GetBoundsForSelectedNodes(Blueprint, Bounds, 50.0f))
	{
		DocumentTemplate->SetBounds(Bounds);
		SpawnLocation.X = DocumentTemplate->NodePosX;
		SpawnLocation.Y = DocumentTemplate->NodePosY;
	}

	UEdGraphNode* NewNode = FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Documentation>(ParentGraph, DocumentTemplate, SpawnLocation, bSelectNewNode);

	// Mark Blueprint as structurally modified since
	// UK2Node_Documentation::NodeCausesStructuralBlueprintChange used to return true
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	return NewNode;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2TargetNode

UEdGraphNode* FEdGraphSchemaAction_K2TargetNode::PerformAction( class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/ ) 
{
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NodeTemplate);
	return NULL;
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_K2PasteHere

UEdGraphNode* FEdGraphSchemaAction_K2PasteHere::PerformAction( class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/ ) 
{
	FKismetEditorUtilities::PasteNodesHere(ParentGraph, Location);
	return NULL;
}

