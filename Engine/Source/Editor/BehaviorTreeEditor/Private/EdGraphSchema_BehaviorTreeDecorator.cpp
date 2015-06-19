// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorActions.h"
#include "GraphEditorSettings.h"
#include "BehaviorTreeConnectionDrawingPolicy.h"
#include "ScopedTransaction.h"
#include "GenericCommands.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeDecoratorSchema"
#define SNAP_GRID (16) // @todo ensure this is the same as SNodePanel::GetSnapGridSize()

namespace DecoratorSchema
{
	// Maximum distance a drag can be off a node edge to require 'push off' from node
	const int32 NodeDistance = 60;
}

UEdGraphNode* FDecoratorSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		NodeTemplate->SetFlags(RF_Transactional);

		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(NULL, ParentGraph, REN_NonTransactional);

		ParentGraph->AddNode(NodeTemplate, true);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();
		NodeTemplate->AllocateDefaultPins();
		NodeTemplate->AutowireNewNode(FromPin);

		// For input pins, new node will generally overlap node being dragged off
		// Work out if we want to visually push away from connected node
		int32 XLocation = Location.X;
		if (FromPin && FromPin->Direction == EGPD_Input)
		{
			UEdGraphNode* PinNode = FromPin->GetOwningNode();
			const float XDelta = FMath::Abs(PinNode->NodePosX - Location.X);

			if (XDelta < DecoratorSchema::NodeDistance)
			{
				// Set location to edge of current node minus the max move distance
				// to force node to push off from connect node enough to give selection handle
				XLocation = PinNode->NodePosX - DecoratorSchema::NodeDistance;
			}
		}

		NodeTemplate->NodePosX = XLocation;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(SNAP_GRID);

		ResultNode = NodeTemplate;
	}

	return ResultNode;
}

UEdGraphNode* FDecoratorSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode) 
{
	UEdGraphNode* ResultNode = NULL;
	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph, FromPins[0], Location);

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

void FDecoratorSchemaAction_NewNode::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( NodeTemplate );
}

//////////////////////////////////////////////////////////////////////////

UEdGraphSchema_BehaviorTreeDecorator::UEdGraphSchema_BehaviorTreeDecorator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PC_Boolean = TEXT("bool");
}

TSharedPtr<FDecoratorSchemaAction_NewNode> UEdGraphSchema_BehaviorTreeDecorator::AddNewDecoratorAction(FGraphContextMenuBuilder& ContextMenuBuilder, const FString& Category, const FText& MenuDesc, const FString& Tooltip)
{
	TSharedPtr<FDecoratorSchemaAction_NewNode> NewAction = TSharedPtr<FDecoratorSchemaAction_NewNode>(new FDecoratorSchemaAction_NewNode(Category, MenuDesc, Tooltip, 0));

	ContextMenuBuilder.AddAction( NewAction );

	return NewAction;
}

void UEdGraphSchema_BehaviorTreeDecorator::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	FGraphNodeCreator<UBehaviorTreeDecoratorGraphNode_Logic> NodeCreator(Graph);
	UBehaviorTreeDecoratorGraphNode_Logic* MyNode = NodeCreator.CreateNode();
	MyNode->LogicMode = EDecoratorLogicMode::Sink;
	SetNodeMetaData(MyNode, FNodeMetadata::DefaultGraphNode);
	NodeCreator.Finalize();
}

void UEdGraphSchema_BehaviorTreeDecorator::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const UBehaviorTreeDecoratorGraphNode* ParentGraphNode = ContextMenuBuilder.FromPin 
		? Cast<UBehaviorTreeDecoratorGraphNode>(ContextMenuBuilder.FromPin->GetOuter()) : NULL;

	FBehaviorTreeEditorModule& EditorModule = FModuleManager::GetModuleChecked<FBehaviorTreeEditorModule>(TEXT("BehaviorTreeEditor"));
	FGraphNodeClassHelper* ClassCache = EditorModule.GetClassCache().Get();

	TArray<FGraphNodeClassData> NodeClasses;
	ClassCache->GatherClasses(UBTDecorator::StaticClass(), NodeClasses);

	for (const auto& NodeClass : NodeClasses)
	{
		const FText NodeTypeName = FText::FromString(NodeClass.ToString());
		TSharedPtr<FDecoratorSchemaAction_NewNode> AddOpAction = AddNewDecoratorAction(ContextMenuBuilder, NodeClass.GetCategory(), NodeTypeName, "");

		UBehaviorTreeDecoratorGraphNode_Decorator* OpNode = NewObject<UBehaviorTreeDecoratorGraphNode_Decorator>(ContextMenuBuilder.OwnerOfTemporaries);
		OpNode->ClassData = NodeClass;
		AddOpAction->NodeTemplate = OpNode;
	}

#if WITH_EDITOR
	UBehaviorTreeDecoratorGraphNode_Logic* LogicCDO = UBehaviorTreeDecoratorGraphNode_Logic::StaticClass()->GetDefaultObject<UBehaviorTreeDecoratorGraphNode_Logic>();
	LogicCDO->GetMenuEntries(ContextMenuBuilder);
#endif
}

void UEdGraphSchema_BehaviorTreeDecorator::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	const UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<const UBehaviorTreeDecoratorGraphNode_Logic>(InGraphNode);
	if (CurrentGraph && !CurrentGraph->bEditable)
	{
		return;
	}

	if (InGraphPin)
	{
		MenuBuilder->BeginSection("DecoratorGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			// Only display the 'Break Links' option if there is a link to break!
			if (InGraphPin->LinkedTo.Num() > 0)
			{
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakPinLinks );

				// add sub menu for break link to
				if(InGraphPin->LinkedTo.Num() > 1)
				{
					MenuBuilder->AddSubMenu(
						LOCTEXT("BreakLinkTo", "Break Link To..." ),
						LOCTEXT("BreakSpecificLinks", "Break a specific link..." ),
						FNewMenuDelegate::CreateUObject( (UEdGraphSchema_BehaviorTreeDecorator*const)this, &UEdGraphSchema_BehaviorTreeDecorator::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
				}
				else
				{
					((UEdGraphSchema_BehaviorTreeDecorator*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
				}
			}
			else
			{
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveExecutionPin);
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode)
	{
		MenuBuilder->BeginSection("DecoratorGraphSchemaNodeActions", LOCTEXT("ClassActionsMenuHeader", "Node Actions"));
		{
			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AddExecutionPin);
			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
			MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
		}
		MenuBuilder->EndSection();
	}
}

void UEdGraphSchema_BehaviorTreeDecorator::GetBreakLinkToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for(TArray<class UEdGraphPin*>::TConstIterator Links(InGraphPin->LinkedTo); Links; ++Links)
	{
		UEdGraphPin* Pin = *Links;
		FString TitleString = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString();
		FText Title = FText::FromString( TitleString );
		if ( Pin->PinName != TEXT("") )
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *Pin->PinName);

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add( TEXT("NodeTitle"), Title );
			Args.Add( TEXT("PinName"), Pin->GetDisplayName() );
			Title = FText::Format( LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args );
		}

		uint32 &Count = LinkTitleCount.FindOrAdd( TitleString );

		FText Description;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeTitle"), Title );
		Args.Add( TEXT("NumberOfNodes"), Count );

		if ( Count == 0 )
		{
			Description = FText::Format( LOCTEXT("BreakDesc", "Break link to {NodeTitle}"), Args );
		}
		else
		{
			Description = FText::Format( LOCTEXT("BreakDescMulti", "Break link to {NodeTitle} ({NumberOfNodes})"), Args );
		}
		++Count;

		MenuBuilder.AddMenuEntry( Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject((USoundClassGraphSchema*const)this, &USoundClassGraphSchema::BreakSinglePinLink, const_cast< UEdGraphPin* >(InGraphPin), *Links) ) );
	}
}

const FPinConnectionResponse UEdGraphSchema_BehaviorTreeDecorator::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	if (PinA == NULL || PinB == NULL)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("One of the Pins is NULL"));
	}

	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are on the same node"));
	}

	// multiple links
	if (PinA->LinkedTo.Num() > 0 || PinB->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
	}

	// Compare the directions
	if (PinA->Direction == PinB->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
}

FLinearColor UEdGraphSchema_BehaviorTreeDecorator::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetDefault<UGraphEditorSettings>()->BooleanPinTypeColor;
}

bool UEdGraphSchema_BehaviorTreeDecorator::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
