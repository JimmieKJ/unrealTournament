// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"

#include "BlueprintUtilities.h"
#include "BlueprintEditorUtils.h"
#include "GraphEditorDragDropAction.h"
#include "BPVariableDragDropAction.h"
#include "SBlueprintPalette.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "VariableDragDropAction"

FKismetVariableDragDropAction::FKismetVariableDragDropAction()
	: VariableName(NAME_None)
	, bControlDrag(false)
	, bAltDrag(false)
{
}

void FKismetVariableDragDropAction::GetLinksThatWillBreak(	UEdGraphNode* Node, UProperty* NewVariableProperty, 
						   TArray<class UEdGraphPin*>& OutBroken)
{
	if(UK2Node_Variable* VarNodeUnderCursor = Cast<UK2Node_Variable>(Node))
	{
		if(const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(VarNodeUnderCursor->GetSchema()) )
		{
			FEdGraphPinType NewPinType;
			Schema->ConvertPropertyToPinType(NewVariableProperty,NewPinType);
			if(UEdGraphPin* Pin = VarNodeUnderCursor->FindPin(VarNodeUnderCursor->GetVarNameString()))
			{
				for(TArray<class UEdGraphPin*>::TIterator i(Pin->LinkedTo);i;i++)
				{
					UEdGraphPin* Link = *i;
					if(false == Schema->ArePinTypesCompatible(NewPinType, Link->PinType))
					{
						OutBroken.Add(Link);
					}
				}
			}
		}
	}
}

void FKismetVariableDragDropAction::HoverTargetChanged()
{
	UProperty* VariableProperty = GetVariableProperty();
	if (VariableProperty == nullptr)
	{
		return;
	}

	FString VariableString = VariableName.ToString();

	// Icon/text to draw on tooltip
	FSlateColor IconColor = FLinearColor::White;
	const FSlateBrush* StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
	FText Message = LOCTEXT("InvalidDropTarget", "Invalid drop target!");

	UEdGraphPin* PinUnderCursor = GetHoveredPin();

	bool bCanMakeSetter = true;
	bool bBadSchema = false;
	bool bBadGraph = false;
	UEdGraph* TheHoveredGraph = GetHoveredGraph();
	if (TheHoveredGraph)
	{
		if (Cast<const UEdGraphSchema_K2>(TheHoveredGraph->GetSchema()) == nullptr)
		{
			bBadSchema = true;
		}
		else if(!CanVariableBeDropped(VariableProperty, *TheHoveredGraph))
		{
			bBadGraph = true;
		}

		UStruct* Outer = CastChecked<UStruct>(VariableProperty->GetOuter());

		FNodeConstructionParams NewNodeParams;
		NewNodeParams.VariableName = VariableName;
		const UBlueprint* DropOnBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TheHoveredGraph);
		NewNodeParams.Graph = TheHoveredGraph;
		NewNodeParams.VariableSource = Outer;
		
		bCanMakeSetter = CanExecuteMakeSetter(NewNodeParams, VariableProperty);
	}

	UEdGraphNode* VarNodeUnderCursor = Cast<UK2Node_Variable>(GetHoveredNode());

	if (bBadSchema)
	{
		StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		Message = LOCTEXT("CannotCreateInThisSchema", "Cannot access variables in this type of graph");
	}
	else if(bBadGraph)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), FText::FromString(VariableString));
		Args.Add(TEXT("Scope"), FText::FromString(TheHoveredGraph->GetName()));

		StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));

		if(IsFromBlueprint(FBlueprintEditorUtils::FindBlueprintForGraph(TheHoveredGraph)) && VariableProperty->GetOuter()->IsA(UFunction::StaticClass()))
		{
			Message = FText::Format( LOCTEXT("IncorrectGraphForLocalVariable_Error", "Cannot place local variable '{VariableName}' in external scope '{Scope}'"), Args);
		}
		else
		{
			Message = FText::Format( LOCTEXT("IncorrectGraphForVariable_Error", "Cannot place variable '{VariableName}' in external scope '{Scope}'"), Args);
		}
	}
	else if (PinUnderCursor != NULL)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinUnderCursor"), FText::FromString(PinUnderCursor->PinName));
		Args.Add(TEXT("VariableName"), FText::FromString(VariableString));

		if(CanVariableBeDropped(VariableProperty, *PinUnderCursor->GetOwningNode()->GetGraph()))
		{
			const UEdGraphSchema_K2* Schema = CastChecked<const UEdGraphSchema_K2>(PinUnderCursor->GetSchema());

			const bool bIsRead = PinUnderCursor->Direction == EGPD_Input;
			const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(PinUnderCursor->GetOwningNode());
			const bool bReadOnlyProperty = FBlueprintEditorUtils::IsPropertyReadOnlyInCurrentBlueprint(Blueprint, VariableProperty);
			const bool bCanWriteIfNeeded = bIsRead || !bReadOnlyProperty;

			FEdGraphPinType VariablePinType;
			Schema->ConvertPropertyToPinType(VariableProperty, VariablePinType);
			const bool bTypeMatch = Schema->ArePinTypesCompatible(VariablePinType, PinUnderCursor->PinType);

			Args.Add(TEXT("PinUnderCursor"), FText::FromString(PinUnderCursor->PinName));

			if (bTypeMatch && bCanWriteIfNeeded)
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));

				if (bIsRead)
				{
					Message = FText::Format(LOCTEXT("MakeThisEqualThat_PinEqualVariableName", "Make {PinUnderCursor} = {VariableName}"), Args);
				}
				else
				{
					Message = FText::Format(LOCTEXT("MakeThisEqualThat_VariableNameEqualPin", "Make {VariableName} = {PinUnderCursor}"), Args);
				}
			}
			else
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				if (!bCanWriteIfNeeded)
				{
					Message = FText::Format(LOCTEXT("ReadOnlyVar_Error", "Cannot write to read-only variable '{VariableName}'"), Args);
				}
				else
				{
					Message = FText::Format(LOCTEXT("NotCompatible_Error", "The type of '{VariableName}' is not compatible with {PinUnderCursor}"), Args);
				}
			}
		}
		else
		{
			Args.Add(TEXT("Scope"), FText::FromString(PinUnderCursor->GetOwningNode()->GetGraph()->GetName()));

			StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			Message = FText::Format( LOCTEXT("IncorrectGraphForPin_Error", "Cannot place local variable '{VariableName}' in external scope '{Scope}'"), Args);
		}
	}
	else if (VarNodeUnderCursor != NULL)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), FText::FromString(VariableString));

		if(CanVariableBeDropped(VariableProperty, *VarNodeUnderCursor->GetGraph()))
		{
			const bool bIsRead = VarNodeUnderCursor->IsA(UK2Node_VariableGet::StaticClass());
			const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(VarNodeUnderCursor);
			const bool bReadOnlyProperty = FBlueprintEditorUtils::IsPropertyReadOnlyInCurrentBlueprint(Blueprint, VariableProperty);
			const bool bCanWriteIfNeeded = bIsRead || !bReadOnlyProperty;

			if (bCanWriteIfNeeded)
			{
				Args.Add(TEXT("ReadOrWrite"), bIsRead ? LOCTEXT("Read", "read") : LOCTEXT("Write", "write"));
				if(WillBreakLinks(VarNodeUnderCursor, VariableProperty))
				{
					StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OKWarn"));
					Message = FText::Format( LOCTEXT("ChangeNodeToWarnBreakLinks", "Change node to {ReadOrWrite} '{VariableName}', WARNING this will break links!"), Args);
				}
				else
				{
					StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
					Message = FText::Format( LOCTEXT("ChangeNodeTo", "Change node to {ReadOrWrite} '{VariableName}'"), Args);
				}
			}
			else
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				Message = FText::Format( LOCTEXT("ReadOnlyVar_Error", "Cannot write to read-only variable '{VariableName}'"), Args);
			}
		}
		else
		{
			Args.Add(TEXT("Scope"), FText::FromString(VarNodeUnderCursor->GetGraph()->GetName()));

			StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			Message = FText::Format( LOCTEXT("IncorrectGraphForNodeReplace_Error", "Cannot replace node with local variable '{VariableName}' in external scope '{Scope}'"), Args);
		}
	}
	else if (!HoveredCategoryName.IsEmpty())
	{
		// Find Blueprint that made this class and get category of variable
		FText Category;
		UBlueprint* Blueprint;
		
		// Find the Blueprint for this property
		if(Cast<UFunction>(VariableSource.Get()))
		{
			Blueprint = UBlueprint::GetBlueprintFromClass(Cast<UClass>(VariableSource->GetOuter()));
		}
		else
		{
			Blueprint = UBlueprint::GetBlueprintFromClass(Cast<UClass>(VariableSource.Get()));
		}

		if(Blueprint != NULL)
		{
			Category = FBlueprintEditorUtils::GetBlueprintVariableCategory(Blueprint, VariableProperty->GetFName(), GetLocalVariableScope() );
		}

		// See if class is native
		UClass* OuterClass = Cast<UClass>(VariableProperty->GetOuter());
		if(OuterClass || Cast<UFunction>(VariableProperty->GetOuter()))
		{
			const bool bIsNativeVar = (OuterClass && OuterClass->ClassGeneratedBy == NULL);

			FFormatNamedArguments Args;
			Args.Add(TEXT("VariableName"), FText::FromString(VariableString));
			Args.Add(TEXT("HoveredCategoryName"), HoveredCategoryName);

			if (bIsNativeVar)
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				Message = FText::Format( LOCTEXT("ChangingCatagoryNotThisVar", "Cannot change category for variable '{VariableName}'"), Args );
			}
			else if (Category.EqualTo(HoveredCategoryName))
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				Message = FText::Format( LOCTEXT("ChangingCatagoryAlreadyIn", "Variable '{VariableName}' is already in category '{HoveredCategoryName}'"), Args );
			}
			else
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
				Message = FText::Format( LOCTEXT("ChangingCatagoryOk", "Move variable '{VariableName}' to category '{HoveredCategoryName}'"), Args );
			}
		}
	}
	else if (HoveredAction.IsValid())
	{
		if(HoveredAction.Pin()->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)HoveredAction.Pin().Get();
			FName TargetVarName = VarAction->GetVariableName();

			// Needs to have a valid index to move it (this excludes variables added through other means, like timelines/components
			int32 MoveVarIndex = INDEX_NONE;
			int32 TargetVarIndex = INDEX_NONE;
			UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Cast<UClass>(VariableSource.Get()));
			if(Blueprint != NULL)
			{
				MoveVarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName);
				TargetVarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, TargetVarName);
			}

			FFormatNamedArguments Args;
			Args.Add(TEXT("VariableName"), FText::FromString(VariableString));
			Args.Add(TEXT("TargetVarName"), FText::FromName(TargetVarName));

			if(MoveVarIndex == INDEX_NONE)
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				Message = FText::Format( LOCTEXT("MoveVarDiffClass", "Cannot reorder variable '{VariableName}'."), Args );
			}
			else if(TargetVarIndex == INDEX_NONE)
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				Message = FText::Format( LOCTEXT("MoveVarOther", "Cannot reorder variable '{VariableName}' before '{TargetVarName}'."), Args );
			}
			else if(VariableName == TargetVarName)
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				Message = FText::Format( LOCTEXT("MoveVarYourself", "Cannot reorder variable '{VariableName}' before itself."), Args );
			}
			else
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
				Message = FText::Format( LOCTEXT("MoveVarOK", "Reorder variable '{VariableName}' before '{TargetVarName}'"), Args );
			}
		}
	}
	else if (bAltDrag && !bCanMakeSetter)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), FText::FromString(VariableString));

		StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		Message = FText::Format(LOCTEXT("CannotPlaceSetter", "Variable '{VariableName}' is readonly, you cannot set this variable."), Args);
	}
	// Draw variable icon
	else
	{
		StatusSymbol = FBlueprintEditor::GetVarIconAndColor(VariableSource.Get(), VariableName, IconColor);
		Message = FText::FromString(VariableString);
	}

	SetSimpleFeedbackMessage(StatusSymbol, IconColor, Message);
}

FReply FKismetVariableDragDropAction::DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	UEdGraphPin* TargetPin = GetHoveredPin();

	if (TargetPin != NULL)
	{
		if (const UEdGraphSchema_K2* Schema = CastChecked<const UEdGraphSchema_K2>(TargetPin->GetSchema()))
		{
			UProperty* VariableProperty = GetVariableProperty();

			if(CanVariableBeDropped(VariableProperty, *TargetPin->GetOwningNode()->GetGraph()))
			{
				const bool bIsRead = TargetPin->Direction == EGPD_Input;
				const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(TargetPin->GetOwningNode());
				const bool bReadOnlyProperty = FBlueprintEditorUtils::IsPropertyReadOnlyInCurrentBlueprint(Blueprint, VariableProperty);
				const bool bCanWriteIfNeeded = bIsRead || !bReadOnlyProperty;

				FEdGraphPinType VariablePinType;
				Schema->ConvertPropertyToPinType(VariableProperty, VariablePinType);
				const bool bTypeMatch = Schema->ArePinTypesCompatible(VariablePinType, TargetPin->PinType);

				if (bTypeMatch && bCanWriteIfNeeded)
				{
					FEdGraphSchemaAction_K2NewNode Action;

					UK2Node_Variable* VarNode = bIsRead ? (UK2Node_Variable*)NewObject<UK2Node_VariableGet>() : (UK2Node_Variable*)NewObject<UK2Node_VariableSet>();
					Action.NodeTemplate = VarNode;

					UBlueprint* DropOnBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetPin->GetOwningNode()->GetGraph());
					UEdGraphSchema_K2::ConfigureVarNode(VarNode, VariableName, VariableSource.Get(), DropOnBlueprint);

					Action.PerformAction(TargetPin->GetOwningNode()->GetGraph(), TargetPin, GraphPosition);
				}
			}
		}
	}

	return FReply::Handled();
}

FReply FKismetVariableDragDropAction::DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	UK2Node_Variable* TargetNode = Cast<UK2Node_Variable>(GetHoveredNode());

	if (TargetNode && (VariableName != TargetNode->GetVarName()))
	{
		const FScopedTransaction Transaction( LOCTEXT("ReplacePinVariable", "Replace Pin Variable") );

		UProperty* VariableProperty = GetVariableProperty();

		if(CanVariableBeDropped(VariableProperty, *TargetNode->GetGraph()))
		{
			const FString OldVarName = TargetNode->GetVarNameString();
			const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(TargetNode->GetSchema());

			TArray<class UEdGraphPin*> BadLinks;
			GetLinksThatWillBreak(TargetNode,VariableProperty,BadLinks);

			// Change the variable name and context
			UBlueprint* DropOnBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetNode->GetGraph());
			UEdGraphPin* Pin = TargetNode->FindPin(OldVarName);
			DropOnBlueprint->Modify();
			TargetNode->Modify();

			if (Pin != NULL)
			{
				Pin->Modify();
			}

			UEdGraphSchema_K2::ConfigureVarNode(TargetNode, VariableName, VariableSource.Get(), DropOnBlueprint);


			if ((Pin == NULL) || (Pin->LinkedTo.Num() == BadLinks.Num()) || (Schema == NULL))
			{
				TargetNode->GetSchema()->ReconstructNode(*TargetNode);
			}
			else 
			{
				FEdGraphPinType NewPinType;
				Schema->ConvertPropertyToPinType(VariableProperty,NewPinType);

				Pin->PinName = VariableName.ToString();
				Pin->PinType = NewPinType;

				//break bad links
				for(TArray<class UEdGraphPin*>::TIterator OtherPinIt(BadLinks);OtherPinIt;++OtherPinIt)
				{
					Pin->BreakLinkTo(*OtherPinIt);
				}
			}

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(DropOnBlueprint);

			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

void FKismetVariableDragDropAction::MakeGetter(FNodeConstructionParams InParams)
{
	check(InParams.Graph);

	const UEdGraphSchema_K2* K2_Schema = Cast<const UEdGraphSchema_K2>(InParams.Graph->GetSchema());
	if (K2_Schema)
	{
		K2_Schema->SpawnVariableGetNode(InParams.GraphPosition, InParams.Graph, InParams.VariableName, InParams.VariableSource.Get());
	}
}

void FKismetVariableDragDropAction::MakeSetter(FNodeConstructionParams InParams)
{
	check(InParams.Graph);

	const UEdGraphSchema_K2* K2_Schema = Cast<const UEdGraphSchema_K2>(InParams.Graph->GetSchema());
	if (K2_Schema)
	{
		K2_Schema->SpawnVariableSetNode(InParams.GraphPosition, InParams.Graph, InParams.VariableName, InParams.VariableSource.Get());
	}
}

bool FKismetVariableDragDropAction::CanExecuteMakeSetter(FNodeConstructionParams InParams, UProperty* InVariableProperty)
{
	check(InVariableProperty);
	check(InParams.VariableSource.Get());

	if(UClass* VariableSourceClass = Cast<UClass>(InParams.VariableSource.Get()))
	{
		const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(InParams.Graph);
		const bool bReadOnlyProperty = FBlueprintEditorUtils::IsPropertyReadOnlyInCurrentBlueprint(Blueprint, InVariableProperty);

		return (!bReadOnlyProperty) && (!VariableSourceClass->HasAnyClassFlags(CLASS_Const));
	}

	return true;
}

FReply FKismetVariableDragDropAction::DroppedOnPanel( const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{	
	if (Cast<const UEdGraphSchema_K2>(Graph.GetSchema()) != NULL)
	{
		UProperty* VariableProperty = GetVariableProperty();
		if (VariableProperty != nullptr && CanVariableBeDropped(VariableProperty, Graph))
		{
			UStruct* Outer = CastChecked<UStruct>(VariableProperty->GetOuter());
			
			FNodeConstructionParams NewNodeParams;
			NewNodeParams.VariableName = VariableName;
			NewNodeParams.Graph = &Graph;
			NewNodeParams.GraphPosition = GraphPosition;
			NewNodeParams.VariableSource= Outer;

			// call analytics
			AnalyticCallback.ExecuteIfBound();

			// Take into account current state of modifier keys in case the user changed his mind
			auto ModifierKeys = FSlateApplication::Get().GetModifierKeys();
			const bool bModifiedKeysActive = ModifierKeys.IsControlDown() || ModifierKeys.IsAltDown();
			const bool bAutoCreateGetter = bModifiedKeysActive ? ModifierKeys.IsControlDown() : bControlDrag;
			const bool bAutoCreateSetter = bModifiedKeysActive ? ModifierKeys.IsAltDown() : bAltDrag;
			// Handle Getter/Setters
			if (bAutoCreateGetter || bAutoCreateSetter)
			{
				if (bAutoCreateGetter || !CanExecuteMakeSetter(NewNodeParams, VariableProperty))
				{
					MakeGetter(NewNodeParams);
					NewNodeParams.GraphPosition.Y += 50.f;
				}
				if (bAutoCreateSetter && CanExecuteMakeSetter( NewNodeParams, VariableProperty))
				{
					MakeSetter(NewNodeParams);
				}
			}
			// Show selection menu
			else
			{
				FMenuBuilder MenuBuilder(true, NULL);
				const FText VariableNameText = FText::FromName( VariableName );

				MenuBuilder.BeginSection("BPVariableDroppedOn", VariableNameText );

				MenuBuilder.AddMenuEntry(
					LOCTEXT("CreateGetVariable", "Get"),
					FText::Format( LOCTEXT("CreateVariableGetterToolTip", "Create Getter for variable '{0}'\n(Ctrl-drag to automatically create a getter)"), VariableNameText ),
					FSlateIcon(),
					FUIAction(
					FExecuteAction::CreateStatic(&FKismetVariableDragDropAction::MakeGetter, NewNodeParams), FCanExecuteAction())
					);

				MenuBuilder.AddMenuEntry(
					LOCTEXT("CreateSetVariable", "Set"),
					FText::Format( LOCTEXT("CreateVariableSetterToolTip", "Create Setter for variable '{0}'\n(Alt-drag to automatically create a setter)"), VariableNameText ),
					FSlateIcon(),
					FUIAction(
					FExecuteAction::CreateStatic(&FKismetVariableDragDropAction::MakeSetter, NewNodeParams),
					FCanExecuteAction::CreateStatic(&FKismetVariableDragDropAction::CanExecuteMakeSetter, NewNodeParams, VariableProperty ))
					);

				TSharedRef< SWidget > PanelWidget = Panel;
				// Show dialog to choose getter vs setter
				FSlateApplication::Get().PushMenu(
					PanelWidget,
					FWidgetPath(),
					MenuBuilder.MakeWidget(),
					ScreenPosition,
					FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu)
					);

				MenuBuilder.EndSection();
			}
		}
	}

	return FReply::Handled();
}



FReply FKismetVariableDragDropAction::DroppedOnAction(TSharedRef<FEdGraphSchemaAction> Action)
{
	if(Action->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
	{
		FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)&Action.Get();

		// Only let you drag and drop if variables are from same BP class, and not onto itself
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Cast<UClass>(VariableSource.Get()));
		FName TargetVarName = VarAction->GetVariableName();
		if( (BP != NULL) && 
			(VariableName != TargetVarName) && 
			(VariableSource == VarAction->GetVariableClass()) )
		{
			bool bMoved = FBlueprintEditorUtils::MoveVariableBeforeVariable(BP, VariableName, TargetVarName, true);
			// If we moved successfully
			if(bMoved)
			{
				// Change category of var to match the one we dragged on to as well
				FText MovedVarCategory = FBlueprintEditorUtils::GetBlueprintVariableCategory(BP, VariableName, GetLocalVariableScope());
				FText TargetVarCategory = FBlueprintEditorUtils::GetBlueprintVariableCategory(BP, TargetVarName, GetLocalVariableScope());
				if(!MovedVarCategory.EqualTo(TargetVarCategory))
				{
					FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, VariableName, GetLocalVariableScope(), TargetVarCategory, true);
				}

				// Update Blueprint after changes so they reflect in My Blueprint tab.
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
			}
		}

		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply FKismetVariableDragDropAction::DroppedOnCategory(FText Category)
{
	UE_LOG(LogTemp, Log, TEXT("Dropped %s on Category %s"), *VariableName.ToString(), *Category.ToString());

	UBlueprint* BP = NULL;
	if(VariableSource.Get()->IsA(UFunction::StaticClass()))
	{
		BP = UBlueprint::GetBlueprintFromClass(Cast<UClass>(VariableSource.Get()->GetOuter()));
	}
	else
	{
		BP = UBlueprint::GetBlueprintFromClass(Cast<UClass>(VariableSource.Get()));
	}

	if(BP != NULL)
	{
		// Check this is actually a different category
		FText CurrentCategory = FBlueprintEditorUtils::GetBlueprintVariableCategory(BP, VariableName, GetLocalVariableScope());
		if(!Category.EqualTo(CurrentCategory))
		{
			FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, VariableName, GetLocalVariableScope(), Category, false);
		}
	}

	return FReply::Handled();
}

bool FKismetVariableDragDropAction::CanVariableBeDropped(const UProperty* InVariableProperty, const UEdGraph& InGraph) const
{
	bool bCanVariableBeDropped = false;
	if (InVariableProperty)
	{
		UObject* Outer = InVariableProperty->GetOuter();

		// Only allow variables to be placed within the same blueprint (otherwise the self context on the dropped node will be invalid)
		bCanVariableBeDropped = IsFromBlueprint(FBlueprintEditorUtils::FindBlueprintForGraph(&InGraph));

		// Local variables have some special conditions for being allowed to be placed
		if (bCanVariableBeDropped && Outer->IsA(UFunction::StaticClass()))
		{
			// Check if the top level graph has the same name as the function, if they do not then the variable cannot be placed in the graph
			if (FBlueprintEditorUtils::GetTopLevelGraph(&InGraph)->GetFName() != Outer->GetFName())
			{
				bCanVariableBeDropped = false;
			}
		}
	}
	return bCanVariableBeDropped;
}

UStruct* FKismetVariableDragDropAction::GetLocalVariableScope() const
{
	if( VariableSource->IsA(UFunction::StaticClass()) )
	{
		return VariableSource.Get();
	}
	return NULL;
}

#undef LOCTEXT_NAMESPACE
