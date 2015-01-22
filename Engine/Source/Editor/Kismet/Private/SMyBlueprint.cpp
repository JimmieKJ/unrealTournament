// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintUtilities.h"
#include "ScopedTransaction.h"
#include "GraphEditor.h"

#include "BlueprintEditor.h"

#include "Editor/PropertyEditor/Public/PropertyEditing.h"

#include "SKismetInspector.h"
#include "SSCSEditor.h"
#include "SMyBlueprint.h"
#include "FindInBlueprints.h"
#include "GraphEditorDragDropAction.h"
#include "BPFunctionDragDropAction.h"
#include "BPVariableDragDropAction.h"
#include "BPDelegateDragDropAction.h"
#include "SBlueprintPalette.h"
#include "SGraphActionMenu.h"
#include "BlueprintEditorCommands.h"

#include "Editor/AnimGraph/Classes/AnimationGraph.h"

#include "BlueprintDetailsCustomization.h"

#include "SBlueprintEditorToolbar.h"

#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"
#include "ObjectEditorUtils.h"
#include "AssetToolsModule.h"
#include "Editor/GraphEditor/Private/GraphActionNode.h"
#include "IDocumentation.h"
#include "Editor/UnrealEd/Public/SourceCodeNavigation.h"
#include "EditorCategoryUtils.h"
#include "SSearchBox.h"
#include "GenericCommands.h"
#include "Components/TimelineComponent.h"
#include "Engine/TimelineTemplate.h"

#define LOCTEXT_NAMESPACE "MyBlueprint"

//////////////////////////////////////////////////////////////////////////

void FMyBlueprintCommands::RegisterCommands() 
{
	UI_COMMAND( OpenGraph, "Open Graph", "Opens up this function, macro, or event graph's graph panel up.", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( OpenGraphInNewTab, "Open in New Tab", "Opens up this function, macro, or event graph's graph panel up in a new tab. Hold down Ctrl and double click for shortcut.", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( FocusNode, "Focus", "Focuses on the associated node", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( FocusNodeInNewTab, "Focus in New Tab", "Focuses on the associated node in a new tab", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( ImplementFunction, "Implement Function", "Implements this overridable function as a new function.", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( FindEntry, "Find References", "Searches for all references of this function or variable.", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND(DeleteEntry, "Delete", "Deletes this function or variable from this blueprint.", EUserInterfaceActionType::Button, FInputGesture(EKeys::Platform_Delete));
	UI_COMMAND( GotoNativeVarDefinition, "Goto Code Definition", "Goto the native code definition of this variable", EUserInterfaceActionType::Button, FInputGesture() );
}

//////////////////////////////////////////////////////////////////////////

class FMyBlueprintCategoryDragDropAction : public FGraphEditorDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMyBlueprintCategoryDragDropAction, FGraphEditorDragDropAction)

	virtual void HoverTargetChanged() override
	{
		const FSlateBrush* StatusSymbol = FEditorStyle::GetBrush(TEXT("NoBrush")); 
		FText Message = FText::FromString(DraggedCategory);

		if (HoveredCategoryName.Len() > 0)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("DraggedCategory"), FText::FromString(DraggedCategory));

			if(HoveredCategoryName == DraggedCategory)
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));

				
				Message = FText::Format( LOCTEXT("MoveCatOverSelf", "Cannot insert category '{DraggedCategory}' before itself."), Args );
			}
			else
			{
				StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
				Args.Add(TEXT("HoveredCategory"), FText::FromString(HoveredCategoryName));
				Message = FText::Format( LOCTEXT("MoveCatOK", "Move category '{DraggedCategory}' before '{HoveredCategory}'"), Args );
			}
		}
		else if (HoveredAction.IsValid())
		{
			StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			Message = LOCTEXT("MoveCatOverAction", "Can only insert before another category.");
		}

		SetSimpleFeedbackMessage(StatusSymbol, FLinearColor::White, Message);
	}
	
	virtual FReply DroppedOnCategory(FString OnCategory) override
	{
		// Get MyBlueprint via MyBlueprintPtr
		TSharedPtr<SMyBlueprint> MyBlueprint = MyBlueprintPtr.Pin();
		if(MyBlueprint.IsValid())
		{
			// Move the category in the blueprint category sort list
			MyBlueprint->MoveCategoryBeforeCategory( DraggedCategory, OnCategory );
		}

		return FReply::Handled();
	}

	static TSharedRef<FMyBlueprintCategoryDragDropAction> New(const FString& InCategory, TSharedPtr<SMyBlueprint> InMyBlueprint)
	{
		TSharedRef<FMyBlueprintCategoryDragDropAction> Operation = MakeShareable(new FMyBlueprintCategoryDragDropAction);
		Operation->DraggedCategory = InCategory;
		Operation->MyBlueprintPtr = InMyBlueprint;
		Operation->Construct();
		return Operation;
	}

	/** Category we were dragging */
	FString DraggedCategory;
	/** MyBlueprint widget we dragged from */
	TWeakPtr<SMyBlueprint>	MyBlueprintPtr;
};

//////////////////////////////////////////////////////////////////////////

void SMyBlueprint::Construct(const FArguments& InArgs, TWeakPtr<FBlueprintEditor> InBlueprintEditor, const UBlueprint* InBlueprint )
{
	BlueprintEditorPtr = InBlueprintEditor;
	EdGraph = nullptr;
	
	TSharedPtr<SWidget> ToolbarBuilderWidget = TSharedPtr<SWidget>();

	if( InBlueprintEditor.IsValid() )
	{
		Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();

		TSharedPtr<FUICommandList> ToolKitCommandList = InBlueprintEditor.Pin()->GetToolkitCommands();

		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().OpenGraph,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnOpenGraph),
			FCanExecuteAction(), FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::CanOpenGraph) );
	
		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().OpenGraphInNewTab,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnOpenGraphInNewTab),
			FCanExecuteAction(), FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::CanOpenGraph) );

		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().FocusNode,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnFocusNode),
			FCanExecuteAction(), FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::CanFocusOnNode) );

		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().FocusNodeInNewTab,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnFocusNodeInNewTab),
			FCanExecuteAction(), FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::CanFocusOnNode) );

		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().ImplementFunction,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnImplementFunction),
			FCanExecuteAction(), FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::CanImplementFunction) );
	
		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().FindEntry,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnFindEntry),
			FCanExecuteAction(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::CanFindEntry) );
	
		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().DeleteEntry,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnDeleteEntry),
			FCanExecuteAction::CreateSP(this, &SMyBlueprint::CanDeleteEntry) );

		ToolKitCommandList->MapAction( FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnDuplicateAction),
			FCanExecuteAction::CreateSP(this, &SMyBlueprint::CanDuplicateAction),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::IsDuplicateActionVisible) );

		ToolKitCommandList->MapAction( FMyBlueprintCommands::Get().GotoNativeVarDefinition,
			FExecuteAction::CreateSP(this, &SMyBlueprint::GotoNativeCodeVarDefinition),
			FCanExecuteAction(),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP(this, &SMyBlueprint::IsNativeVariable) );

		TSharedPtr<FBlueprintEditorToolbar> Toolbar = MakeShareable(new FBlueprintEditorToolbar(InBlueprintEditor.Pin()));
		TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);
		Toolbar->AddNewToolbar(Extender);

		FToolBarBuilder ToolbarBuilder(ToolKitCommandList, FMultiBoxCustomization::None, Extender);
		ToolbarBuilder.BeginSection("MyBlueprint");
		ToolbarBuilder.EndSection();
		ToolbarBuilderWidget = ToolbarBuilder.MakeWidget();

		ToolKitCommandList->MapAction(FGenericCommands::Get().Rename,
			FExecuteAction::CreateSP(this, &SMyBlueprint::OnRequestRenameOnActionNode),
			FCanExecuteAction::CreateSP(this, &SMyBlueprint::CanRequestRenameOnActionNode));
	}
	else
	{
		// we're in read only mode when there's no blueprint editor:
		Blueprint = const_cast<UBlueprint*>(InBlueprint);
		check(Blueprint);
		ToolbarBuilderWidget = SNew(SBox);
	}

	SAssignNew(FilterBox, SSearchBox)
		.OnTextChanged( this, &SMyBlueprint::OnFilterTextChanged );

	// create the main action list piece of this widget
	TSharedRef<SWidget> MyBlueprintActionMenu = SAssignNew(GraphActionMenu, SGraphActionMenu, false)
		.OnGetFilterText(this, &SMyBlueprint::GetFilterText)
		.OnCreateWidgetForAction(this, &SMyBlueprint::OnCreateWidgetForAction)
		.OnCollectAllActions(this, &SMyBlueprint::CollectAllActions)
		.OnActionDragged(this, &SMyBlueprint::OnActionDragged)
		.OnCategoryDragged(this, &SMyBlueprint::OnCategoryDragged)
		.OnActionSelected(this, &SMyBlueprint::OnGlobalActionSelected)
		.OnActionDoubleClicked(this, &SMyBlueprint::OnActionDoubleClicked)
		.OnContextMenuOpening(this, &SMyBlueprint::OnContextMenuOpening)
		.OnCategoryTextCommitted(this, &SMyBlueprint::OnCategoryNameCommitted)
		.OnCanRenameSelectedAction(this, &SMyBlueprint::CanRequestRenameOnActionNode)
		.OnGetSectionTitle(this,&SMyBlueprint::OnGetSectionTitle)
		.AlphaSortItems(false);

	// now piece together all the content for this widget
	ChildSlot
	[
		SNew(SBorder)
		.Padding(4.0f)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("MyBlueprintPanel")))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ToolbarBuilderWidget.ToSharedRef()
			]
				
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				FilterBox.ToSharedRef()
			]
				
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(ActionMenuContainer, SSplitter)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot()
				[
					MyBlueprintActionMenu
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(3.0f, 2.0f, 0.0f, 0.0f))
			[
				SNew(SCheckBox)
				.IsChecked(this, &SMyBlueprint::OnUserVarsCheckState)
				.OnCheckStateChanged(this, &SMyBlueprint::OnUserVarsCheckStateChanged)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowInheritedVariables", "Show inherited variables"))
					.ToolTip(IDocumentation::Get()->CreateToolTip(
						LOCTEXT("ShowInheritedVariablesTooltip", "Should inherited variables from parent classes and blueprints be shown in the tree?"),
						NULL,
						TEXT("Shared/Editors/BlueprintEditor"),
						TEXT("MyBlueprint_ShowInheritedVariables")))
				]
			]
		]
	];

	if (GetLocalActionsListVisibility() == EVisibility::Visible)
	{
		ActionMenuContainer->AddSlot()
			.Value(0.33)
		[
			ConstructLocalActionPanel()
		];
	}
	
	ResetLastPinType();

	if( !BlueprintEditorPtr.IsValid() )
	{
		Refresh();
	}
}

void SMyBlueprint::OnCategoryNameCommitted(const FText& InNewText, ETextCommit::Type InTextCommit, TWeakPtr< FGraphActionNode > InAction )
{
	// Remove excess whitespace and prevent categories with just spaces
	FText CategoryName = FText::TrimPrecedingAndTrailing(InNewText);

	TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
	GraphActionMenu->GetCategorySubActions(InAction, Actions);

	if (Actions.Num())
	{
		const FScopedTransaction Transaction( LOCTEXT( "RenameCategory", "Rename Category" ) );

		GetBlueprintObj()->Modify();

		for (int32 i = 0; i < Actions.Num(); ++i)
		{
			if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
			{
				FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)Actions[i].Get();

				if(UProperty* TargetProperty = VarAction->GetProperty())
				{
					UClass* OuterClass = CastChecked<UClass>(VarAction->GetProperty()->GetOuter());
					const bool bIsNativeVar = (OuterClass->ClassGeneratedBy == NULL);

					// If the variable is not native and it's outer is the skeleton generated class, we can rename the category
					if(!bIsNativeVar && OuterClass == GetBlueprintObj()->SkeletonGeneratedClass)
					{
						FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), VarAction->GetVariableName(), NULL, FName( *CategoryName.ToString() ), true);
					}
				}
			}
			else if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2LocalVar::StaticGetTypeId())
			{
				FEdGraphSchemaAction_K2LocalVar* LocalVarAction = (FEdGraphSchemaAction_K2LocalVar*)Actions[i].Get();

				FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), LocalVarAction->GetVariableName(), LocalVarAction->GetVariableScope(), FName( *CategoryName.ToString() ), true);
			}
			else if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
			{
				FEdGraphSchemaAction_K2Delegate* DelegateAction = (FEdGraphSchemaAction_K2Delegate*)Actions[i].Get();
				FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), DelegateAction->GetDelegatePoperty()->GetFName(), NULL, FName( *CategoryName.ToString() ), true);
			}
			else if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
			{
				// Do not allow renaming of any graph actions outside of the following
				if(Actions[i]->SectionID == NodeSectionID::FUNCTION || Actions[i]->SectionID == NodeSectionID::MACRO)
				{
					FEdGraphSchemaAction_K2Graph* GraphAction = (FEdGraphSchemaAction_K2Graph*)Actions[i].Get();

					// Don't allow changing the category of a graph who's parent is not the current Blueprint
					if(!FBlueprintEditorUtils::IsPaletteActionReadOnly(Actions[i], BlueprintEditorPtr.Pin()) && FBlueprintEditorUtils::FindBlueprintForGraph(GraphAction->EdGraph) == GetBlueprintObj())
					{
						TWeakObjectPtr<UK2Node_EditablePinBase> EntryNode;
						TWeakObjectPtr<UK2Node_EditablePinBase> ResultNode;
						FBlueprintEditorUtils::GetEntryAndResultNodes(GraphAction->EdGraph, EntryNode, ResultNode);
						EntryNode->Modify();
						if (UK2Node_FunctionEntry* FunctionEntryNode = Cast<UK2Node_FunctionEntry>(EntryNode.Get()))
						{
							FunctionEntryNode->MetaData.Category = CategoryName.ToString();
						}
						else if (UK2Node_Tunnel* TypedEntryNode = ExactCast<UK2Node_Tunnel>(EntryNode.Get()))
						{
							TypedEntryNode->MetaData.Category = CategoryName.ToString();
						}

						if(UFunction* Function = GetBlueprintObj()->SkeletonGeneratedClass->FindFunctionByName(GraphAction->EdGraph->GetFName()))
						{
							Function->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *CategoryName.ToString());
						}
					}
				}
			}
		}
		Refresh();
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprintObj());
		BlueprintEditorPtr.Pin()->GetMyBlueprintWidget()->SelectItemByName(FName(*CategoryName.ToString()), ESelectInfo::OnMouseClick, InAction.Pin()->SectionID, true);
	}
}

FText SMyBlueprint::OnGetSectionTitle( int32 InSectionID )
{
	FText SeperatorTitle;
	/* Setup an appropriate name for the section for this node */
	switch( InSectionID )
	{
	case NodeSectionID::VARIABLE:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Variables", "Variables");
		break;
	case NodeSectionID::COMPONENT:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Components", "Components");
		break;
	case NodeSectionID::FUNCTION:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Functions", "Functions");
		break;
	case NodeSectionID::FUNCTION_OVERRIDABLE:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "OverridableFunctions", "Overridable Functions");
		break;
	case NodeSectionID::MACRO:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Macros", "Macros");
		break;
	case NodeSectionID::INTERFACE:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Interfaces", "Interfaces");
		break;
	case NodeSectionID::DELEGATE:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "EventDispatchers", "Event Dispatchers");
		break;	
	case NodeSectionID::GRAPH:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Graphs", "Graphs");
		break;
	case NodeSectionID::USER_ENUM:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Userenums", "User Enums");
		break;	
	case NodeSectionID::LOCAL_VARIABLE:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "LocalVariables", "Local Variables");
		break;	
	case NodeSectionID::USER_STRUCT:
		SeperatorTitle = NSLOCTEXT("GraphActionNode", "Userstructs", "User Structs");
		break;	
	default:
	case NodeSectionID::NONE:
		SeperatorTitle = FText::GetEmpty();
		break;
	}
	return SeperatorTitle;
}

bool SMyBlueprint::CanRequestRenameOnActionNode(TWeakPtr<FGraphActionNode> InSelectedNode) const
{
	bool bIsReadOnly = true;

	// If checking if renaming is available on a category node, the category must have a non-native variable.
	if(InSelectedNode.Pin()->IsCategoryNode())
	{
		TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
		GraphActionMenu->GetCategorySubActions(InSelectedNode, Actions);

		for (int32 i = 0; i < Actions.Num(); ++i)
		{
			if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
			{
				FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)Actions[i].Get();

				UClass* SkeletonGeneratedClass = GetBlueprintObj()->SkeletonGeneratedClass;
				if (UProperty* TargetProperty = FindField<UProperty>(SkeletonGeneratedClass, VarAction->GetVariableName()))
				{
					UClass* OuterClass = CastChecked<UClass>(TargetProperty->GetOuter());
					const bool bIsNativeVar = (OuterClass->ClassGeneratedBy == NULL);

					// If the variable is not native and it's outer is the skeleton generated class, we can rename the category
					if(!bIsNativeVar && OuterClass == GetBlueprintObj()->SkeletonGeneratedClass)
					{
						bIsReadOnly = false;
						break;
					}
				}
			}
			else if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
			{
				FEdGraphSchemaAction_K2Graph* GraphAction = (FEdGraphSchemaAction_K2Graph*)(Actions[i].Get());

				// Do not allow renaming of any graph actions outside of the following
				if(Actions[i]->SectionID == NodeSectionID::FUNCTION || Actions[i]->SectionID == NodeSectionID::MACRO)
				{
					if(!FBlueprintEditorUtils::IsPaletteActionReadOnly(Actions[i], BlueprintEditorPtr.Pin()) && FBlueprintEditorUtils::FindBlueprintForGraph(GraphAction->EdGraph) == GetBlueprintObj())
					{
						bIsReadOnly = false;
						break;
					}
				}
			}
			else if(Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
			{
				if(!FBlueprintEditorUtils::IsPaletteActionReadOnly(Actions[i], BlueprintEditorPtr.Pin()))
				{
					bIsReadOnly = false;
					break;
				}
			}
		}
	}
	else if(InSelectedNode.Pin()->IsActionNode())
	{
		check( InSelectedNode.Pin()->Actions.Num() > 0 && InSelectedNode.Pin()->Actions[0].IsValid() );
		bIsReadOnly = FBlueprintEditorUtils::IsPaletteActionReadOnly(InSelectedNode.Pin()->Actions[0], BlueprintEditorPtr.Pin());
	}

	return BlueprintEditorPtr.Pin()->InEditingMode() && !bIsReadOnly;
}

void SMyBlueprint::Refresh()
{
	GraphActionMenu->RefreshAllActions(true);
	
	bool bLocalActionsAreVisible = (GetLocalActionsListVisibility() == EVisibility::Visible);
	if (bLocalActionsAreVisible)
	{
		if (!LocalGraphActionMenu.IsValid())
		{
			ActionMenuContainer->AddSlot()
				.Value(0.33f)
			[
				ConstructLocalActionPanel()
			];
		}
		check(LocalGraphActionMenu.IsValid());
		LocalGraphActionMenu->RefreshAllActions(true);
	}
	else if (LocalGraphActionMenu.IsValid() && ensure(ActionMenuContainer->GetChildren()->Num() > 1))
	{
		ActionMenuContainer->RemoveAt(1);
		LocalGraphActionMenu = NULL;
	}
}

TSharedRef<SWidget> SMyBlueprint::OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData)
{
	return BlueprintEditorPtr.IsValid() ? SNew(SBlueprintPaletteItem, InCreateData, BlueprintEditorPtr.Pin()) : SNew(SBlueprintPaletteItem, InCreateData, GetBlueprintObj());
}

TSharedRef<SWidget> SMyBlueprint::ConstructLocalActionPanel()
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew( SBorder )
				.Visibility(this, &SMyBlueprint::GetLocalActionsListVisibility)
				.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryTop") )
				.BorderBackgroundColor( FLinearColor( .6,.6,.6, 1.0f ) )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("LocalVariables", "Local Variables"))
				]
				+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
				[
					SNew(SButton)
						.OnClicked(this, &SMyBlueprint::OnAddNewLocalVariable)
						.ButtonStyle( FEditorStyle::Get(), "ToggleButton")
						.ToolTipText(LOCTEXT("AddNewLocalVar_Tooltip", "Adds a new local variable"))
					[
						SNew(SImage)
							.Image(FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray"))
					]
				]
			]
		]
		+SVerticalBox::Slot()
			.FillHeight(1.0f)
		[
			SAssignNew(LocalGraphActionMenu, SGraphActionMenu, false)
				.Visibility(this, &SMyBlueprint::GetLocalActionsListVisibility)
				.OnGetFilterText(this, &SMyBlueprint::GetFilterText)
				.OnCreateWidgetForAction(this, &SMyBlueprint::OnCreateWidgetForAction)
				.OnCollectAllActions(this, &SMyBlueprint::GetLocalVariables)
				.OnActionDragged(this, &SMyBlueprint::OnActionDragged)
				.OnCategoryDragged(this, &SMyBlueprint::OnCategoryDragged)
				.OnActionSelected(this, &SMyBlueprint::OnLocalActionSelected)
				.OnActionDoubleClicked(this, &SMyBlueprint::OnActionDoubleClicked)
				.OnContextMenuOpening(this, &SMyBlueprint::OnContextMenuOpening)
				.OnCategoryTextCommitted(this, &SMyBlueprint::OnCategoryNameCommitted)
				.OnCanRenameSelectedAction(this, &SMyBlueprint::CanRequestRenameOnActionNode)
				.OnGetSectionTitle(this,&SMyBlueprint::OnGetSectionTitle)
				.AlphaSortItems(false)
		];

}

void SMyBlueprint::GetChildGraphs(UEdGraph* EdGraph, FGraphActionListBuilderBase& OutAllActions, FString ParentCategory)
{
	// Grab subgraphs
	const UEdGraphSchema* Schema = EdGraph->GetSchema();

	// Grab display info
	FGraphDisplayInfo EdGraphDisplayInfo;
	Schema->GetGraphDisplayInformation(*EdGraph, EdGraphDisplayInfo);
	const FText EdGraphDisplayName = EdGraphDisplayInfo.DisplayName;

	// Grab children graphs
	for (TArray<UEdGraph*>::TIterator It(EdGraph->SubGraphs); It; ++It)
	{
		UEdGraph* Graph = *It;
		check(Graph);

		const UEdGraphSchema* ChildSchema = Graph->GetSchema();
		FGraphDisplayInfo ChildGraphDisplayInfo;
		ChildSchema->GetGraphDisplayInformation(*Graph, ChildGraphDisplayInfo);

		FText DisplayText = ChildGraphDisplayInfo.DisplayName;

		FString Category = ((ParentCategory.IsEmpty()) ? "" : ParentCategory + "|") + EdGraphDisplayName.ToString();
		FString FunctionTooltip = DisplayText.ToString();
		FText FunctionDesc = DisplayText;
		const FName DisplayName =  FName(*DisplayText.ToString());

		TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Subgraph, Category, FunctionDesc, FunctionTooltip, 1));
		NewFuncAction->FuncName = DisplayName;
		NewFuncAction->EdGraph = Graph;
		NewFuncAction->SectionID = NodeSectionID::FUNCTION;
		OutAllActions.AddAction(NewFuncAction);
		
		GetChildGraphs(Graph, OutAllActions, Category);
	}
}

void SMyBlueprint::GetChildEvents(UEdGraph const* EdGraph, int32 const SectionId, FGraphActionListBuilderBase& OutAllActions) const
{
	if (!ensure(EdGraph != NULL))
	{
		return;
	}

	// grab the parent graph's name
	UEdGraphSchema const* Schema = EdGraph->GetSchema();
	FGraphDisplayInfo EdGraphDisplayInfo;
	Schema->GetGraphDisplayInformation(*EdGraph, EdGraphDisplayInfo);
	FText const EdGraphDisplayName = EdGraphDisplayInfo.DisplayName;

	TArray<UK2Node_Event*> EventNodes;
	EdGraph->GetNodesOfClass<UK2Node_Event>(EventNodes);

	FString const ActionCategory = EdGraphDisplayName.ToString();
	for (auto It = EventNodes.CreateConstIterator(); It; ++It)
	{
		UK2Node_Event* const EventNode = (*It);

		FString const Tooltip = EventNode->GetTooltipText().ToString();
		FText const Description = EventNode->GetNodeTitle(ENodeTitleType::EditableTitle);

		TSharedPtr<FEdGraphSchemaAction_K2Event> EventNodeAction = MakeShareable(new FEdGraphSchemaAction_K2Event(ActionCategory, Description, Tooltip, 0));
		EventNodeAction->NodeTemplate = EventNode;
		EventNodeAction->SectionID = SectionId;
		OutAllActions.AddAction(EventNodeAction);
	}
}

void SMyBlueprint::GetLocalVariables(FGraphActionListBuilderBase& OutAllActions) const
{
	// We want to pull local variables from the top level function graphs
	UEdGraph* EdGraph = FBlueprintEditorUtils::GetTopLevelGraph(GetFocusedGraph());
	if( EdGraph )
	{
		// grab the parent graph's name
		UEdGraphSchema const* Schema = EdGraph->GetSchema();
		FGraphDisplayInfo EdGraphDisplayInfo;
		Schema->GetGraphDisplayInformation(*EdGraph, EdGraphDisplayInfo);
		FText const EdGraphDisplayName = EdGraphDisplayInfo.DisplayName;

		TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
		EdGraph->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);

		// Search in all FunctionEntry nodes for their local variables
		FString ActionCategory;
		for (UK2Node_FunctionEntry* const FunctionEntry : FunctionEntryNodes)
		{
			for( const FBPVariableDescription& Variable : FunctionEntry->LocalVariables )
			{
				FString Category = Variable.Category.ToString();
				if (Variable.Category == GetDefault<UEdGraphSchema_K2>()->VR_DefaultCategory)
				{
					Category = FString();
				}

				UFunction* Func = FindField<UFunction>(GetBlueprintObj()->SkeletonGeneratedClass, EdGraph->GetFName());
				if (Func)
				{
					TSharedPtr<FEdGraphSchemaAction_K2LocalVar> NewVarAction = MakeShareable(new FEdGraphSchemaAction_K2LocalVar(Category, FText::FromName(Variable.VarName), TEXT(""), 0));
					NewVarAction->SetVariableInfo(Variable.VarName, Func);
					OutAllActions.AddAction(NewVarAction);
				}
			}
		}
	}
}

EVisibility SMyBlueprint::GetLocalActionsListVisibility() const
{
	if( !BlueprintEditorPtr.IsValid())
	{
		return EVisibility::Visible;
	}

	if( BlueprintEditorPtr.IsValid() && BlueprintEditorPtr.Pin()->NewDocument_IsVisibleForType(FBlueprintEditor::CGT_NewLocalVariable))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

void SMyBlueprint::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UBlueprint* Blueprint = GetBlueprintObj();
	check(Blueprint);

	EFieldIteratorFlags::SuperClassFlags FieldIteratorSuperFlag = EFieldIteratorFlags::IncludeSuper;
	if(bShowUserVarsOnly)
	{
		FieldIteratorSuperFlag = EFieldIteratorFlags::ExcludeSuper;
	}

	// Helper structure to aid category sorting
	struct FGraphActionSort
	{
		public:
			FGraphActionSort( TArray<FName>& BlueprintCategorySorting )
				: bCategoriesModified( false )
				, CategorySortIndices( BlueprintCategorySorting )
			{
				CategoryUsage.Init( 0, CategorySortIndices.Num() );
			}

			void AddAction( const FString& Category, TSharedPtr<FEdGraphSchemaAction> Action )
			{
				// Find root category
				int32 RootCategoryDelim = Category.Find( TEXT( "|" ));
				FName RootCategory = RootCategoryDelim == INDEX_NONE ?	*Category : *Category.Left( RootCategoryDelim );
				// Get root sort index
				const int32 SortIndex = GetSortIndex( RootCategory ) + Action->SectionID;

				static const FName DefaultCategory( TEXT( "Default" ));
				if( RootCategory != DefaultCategory )
				{
					// Strip sections out of non default categories to enable them to be grouped together
					Action->SectionID = NodeSectionID::USER_SORTED;
				}
				SortedActions.Add( SortIndex, Action );
			}

			void GetAllActions( FGraphActionListBuilderBase& OutActions )
			{
				SortedActions.KeySort( TLess<int32>() );

				for( auto Iter : SortedActions )
				{
					OutActions.AddAction( Iter.Value );
				}
			}

			void CleanupCategories()
			{
				// Scrub unused categories from the blueprint
				if( bCategoriesModified )
				{
					for( int32 CategoryIdx = CategoryUsage.Num() - 1; CategoryIdx >= 0; CategoryIdx-- )
					{
						if( CategoryUsage[ CategoryIdx ] == 0 )
						{
							CategorySortIndices.RemoveAt( CategoryIdx );
						}
					}
					bCategoriesModified = false;
				}
			}

		private:
			const int32 GetSortIndex( FName Category )
			{
				int32 SortIndex = CategorySortIndices.Find( Category );

				if( SortIndex == INDEX_NONE )
				{
					bCategoriesModified = true;
					SortIndex = CategorySortIndices.Add( Category );
					CategoryUsage.Add( 0 );
				}
				CategoryUsage[ SortIndex ]++;
				// Spread the sort values so we can fine tune sorting
				SortIndex *= 1000;

				return SortIndex + SortedActions.Num();
			}

		private:
			/** Signals if the blueprint categories have been modified and require cleanup */
			bool bCategoriesModified;
			/** Tracks category usage to aid removal of unused categories */
			TArray<int32> CategoryUsage;
			/** Reference to the category sorting in the blueprint */
			TArray<FName>& CategorySortIndices;
			/** Map used to sort Graph actions */
			TMultiMap<int32,TSharedPtr<FEdGraphSchemaAction>> SortedActions;
	}; 

	// Initialise action sorting instance
	FGraphActionSort SortList( Blueprint->CategorySorting );
	// List of names of functions we implement
	TArray<FName> ImplementedFunctions;
	// Grab Variables
	for (TFieldIterator<UProperty> PropertyIt(Blueprint->SkeletonGeneratedClass, FieldIteratorSuperFlag); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		FName PropName = Property->GetFName();
		
		// Don't show delegate properties, there is special handling for these
		const bool bMulticastDelegateProp = Property->IsA(UMulticastDelegateProperty::StaticClass());
		const bool bDelegateProp = (Property->IsA(UDelegateProperty::StaticClass()) || bMulticastDelegateProp);
		const bool bShouldShowAsVar = (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintVisible)) && !bDelegateProp;
		const bool bShouldShowAsDelegate = !Property->HasAnyPropertyFlags(CPF_Parm) && bMulticastDelegateProp 
			&& Property->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
		UObjectPropertyBase* Obj = Cast<UObjectPropertyBase>(Property);
		if(!bShouldShowAsVar && !bShouldShowAsDelegate)
		{
			continue;
		}

		const FText PropertyTooltip = Property->GetToolTipText();
		const FName PropertyName = Property->GetFName();
		const FText PropertyDesc = FText::FromName(PropertyName);

		FName CategoryName = FObjectEditorUtils::GetCategoryFName(Property);
		FString PropertyCategory = FObjectEditorUtils::GetCategory(Property);
		const FString UserCategoryName = FEditorCategoryUtils::GetCategoryDisplayString( PropertyCategory );

		if ((CategoryName == Blueprint->GetFName()) || (CategoryName == K2Schema->VR_DefaultCategory))
		{
			CategoryName = NAME_None;		// default, so place in 'non' category
			PropertyCategory = FString();
		}

		if (bShouldShowAsVar)
		{
			TSharedPtr<FEdGraphSchemaAction_K2Var> NewVarAction = MakeShareable(new FEdGraphSchemaAction_K2Var(PropertyCategory, PropertyDesc, PropertyTooltip.ToString(), 0));
			NewVarAction->SetVariableInfo(PropertyName, Blueprint->SkeletonGeneratedClass);
			const bool bComponentProperty = Obj && Obj->PropertyClass ? Obj->PropertyClass->IsChildOf<UActorComponent>() : false;
			NewVarAction->SectionID = bComponentProperty ? NodeSectionID::COMPONENT : NodeSectionID::VARIABLE;
			SortList.AddAction( UserCategoryName, NewVarAction );
		}
		else if (bShouldShowAsDelegate)
		{
			TSharedPtr<FEdGraphSchemaAction_K2Delegate> NewFuncAction;
			// Delegate is visible in MyBlueprint when not-native or its category name is not empty.
			if (Property->HasAllPropertyFlags(CPF_Edit) || !PropertyCategory.IsEmpty())
			{
				NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Delegate(PropertyCategory, PropertyDesc, PropertyTooltip.ToString(), 0));
				NewFuncAction->SetDelegateInfo(PropertyName, Blueprint->SkeletonGeneratedClass);
				NewFuncAction->EdGraph = NULL;
				NewFuncAction->SectionID = NodeSectionID::DELEGATE;
				SortList.AddAction( UserCategoryName, NewFuncAction );
			}

			UClass* OwnerClass = CastChecked<UClass>(Property->GetOuter());
			UEdGraph* Graph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(Blueprint, PropertyName);
			if (Graph && OwnerClass && (Blueprint == OwnerClass->ClassGeneratedBy))
			{
				if ( NewFuncAction.IsValid() )
				{
					NewFuncAction->EdGraph = Graph;
				}
				ImplementedFunctions.AddUnique(PropertyName);
			}
		}
	}

	// Add all the sorted variables and components
	SortList.CleanupCategories();
	SortList.GetAllActions( OutAllActions );

	// Grab functions implemented by the blueprint
	for (int32 i = 0; i < Blueprint->FunctionGraphs.Num(); i++)
	{
		UEdGraph* Graph = Blueprint->FunctionGraphs[i];
		check(Graph);

		bool bIsConstructionScript = Graph->GetFName() == K2Schema->FN_UserConstructionScript;

		FGraphDisplayInfo DisplayInfo;
		Graph->GetSchema()->GetGraphDisplayInformation(*Graph, DisplayInfo);

		FString FunctionCategory;
		if(UFunction* Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName()))
		{
			FunctionCategory = Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory);
		}

		TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Function, FunctionCategory, DisplayInfo.PlainName, DisplayInfo.Tooltip, bIsConstructionScript ? 2 : 1));
		NewFuncAction->FuncName = Graph->GetFName();
		NewFuncAction->EdGraph = Graph;
		NewFuncAction->SectionID = NodeSectionID::FUNCTION;

		//@TODO: Should be a bit more generic (or the AnimGraph shouldn't be stored as a FunctionGraph...)
		if (Graph->IsA<UAnimationGraph>())
		{
			NewFuncAction->SectionID = NodeSectionID::GRAPH;
		}

		OutAllActions.AddAction(NewFuncAction);

		GetChildGraphs(Graph, OutAllActions);
		GetChildEvents(Graph, NewFuncAction->SectionID, OutAllActions);

		ImplementedFunctions.AddUnique(Graph->GetFName());
	}
	// Grab macros implemented by the blueprint
	for (int32 i = 0; i < Blueprint->MacroGraphs.Num(); i++)
	{
		UEdGraph* Graph = Blueprint->MacroGraphs[i];
		check(Graph);
		
		const FName FunctionName = Graph->GetFName();

		FGraphDisplayInfo DisplayInfo;
		Graph->GetSchema()->GetGraphDisplayInformation(*Graph, DisplayInfo);

		FString MacroCategory = GetGraphCategory(Graph);

		TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Macro, MacroCategory, DisplayInfo.PlainName, DisplayInfo.Tooltip, 1));
		NewFuncAction->SectionID = NodeSectionID::MACRO;
		NewFuncAction->FuncName = FunctionName;
		NewFuncAction->EdGraph = Graph;
		OutAllActions.AddAction(NewFuncAction);

		GetChildGraphs(Graph, OutAllActions);
		GetChildEvents(Graph, NewFuncAction->SectionID, OutAllActions);

		ImplementedFunctions.AddUnique(FunctionName);
	}

	// Show potentially overridable functions
	for (TFieldIterator<UFunction> FunctionIt(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		const FName FunctionName = Function->GetFName();

		if (UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && !ImplementedFunctions.Contains(FunctionName) && !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function))
		{
			FString FunctionTooltip = UK2Node_CallFunction::GetDefaultTooltipForFunction(Function);
			FText FunctionDesc = FText::FromString(Function->GetMetaData(TEXT("FriendlyName")));
			if (FunctionDesc.IsEmpty())
			{
				FunctionDesc = FText::FromString(Function->GetName());
			}

			FString FunctionCategory = Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory);

			TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Function, FunctionCategory, FunctionDesc, FunctionTooltip, 1));
			NewFuncAction->FuncName = FunctionName;
			NewFuncAction->SectionID = NodeSectionID::FUNCTION_OVERRIDABLE;
			OutAllActions.AddAction(NewFuncAction);
		}
	}

	// Also function implemented for interfaces
	for (int32 i=0; i < Blueprint->ImplementedInterfaces.Num(); i++)
	{
		FBPInterfaceDescription& InterfaceDesc = Blueprint->ImplementedInterfaces[i];
		for (int32 FuncIdx = 0; FuncIdx < InterfaceDesc.Graphs.Num(); FuncIdx++)
		{
			UEdGraph* Graph = InterfaceDesc.Graphs[FuncIdx];
			check(Graph);
		
			const FName FunctionName = Graph->GetFName();
			FString FunctionTooltip = FunctionName.ToString();
			FString FunctionDesc = FunctionName.ToString();
			
			FString FunctionCategory;
			if(UFunction* Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(Graph->GetFName()))
			{
				FunctionCategory = Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory);
			}

			TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Interface, FunctionCategory, FText::FromString(FunctionDesc), FunctionTooltip, 1));
			NewFuncAction->FuncName = FunctionName;
			NewFuncAction->EdGraph = Graph;
			NewFuncAction->SectionID = NodeSectionID::INTERFACE;
			OutAllActions.AddAction(NewFuncAction);

			GetChildGraphs(Graph, OutAllActions);
			GetChildEvents(Graph, NewFuncAction->SectionID, OutAllActions);
		}
	}

	// also walk up the class chain to look for overridable functions in natively implemented interfaces
	for ( UClass* TempClass=Blueprint->ParentClass; TempClass; TempClass=TempClass->GetSuperClass() )
	{
		for (int32 Idx=0; Idx<TempClass->Interfaces.Num(); ++Idx)
		{
			FImplementedInterface const& I = TempClass->Interfaces[Idx];
			if (!I.bImplementedByK2)
			{
				// same as above, make a function?
				for (TFieldIterator<UFunction> FunctionIt(I.Class, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
				{
					const UFunction* Function = *FunctionIt;
					const FName FunctionName = Function->GetFName();

					if (UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && !ImplementedFunctions.Contains(FunctionName) && !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function))
					{
						FString FunctionTooltip = Function->GetToolTipText().ToString();
						FString FunctionDesc = Function->GetMetaData(TEXT("FriendlyName"));
						if (FunctionDesc.IsEmpty()) {FunctionDesc = Function->GetName();}

						FString FunctionCategory = Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory);

						TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Function, FunctionCategory, FText::FromString(FunctionDesc), FunctionTooltip, 1));
						NewFuncAction->FuncName = FunctionName;
						OutAllActions.AddAction(NewFuncAction);
					}
				}
			}
		}
	}

	// Grab ubergraph pages
	for (int32 i = 0; i < Blueprint->UbergraphPages.Num(); i++)
	{
		UEdGraph* Graph = Blueprint->UbergraphPages[i];
		check(Graph);
		
		FGraphDisplayInfo DisplayInfo;
		Graph->GetSchema()->GetGraphDisplayInformation(*Graph, DisplayInfo);

		TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Graph, FString(), DisplayInfo.PlainName, DisplayInfo.Tooltip, 2));
		NewFuncAction->FuncName = Graph->GetFName();
		NewFuncAction->EdGraph = Graph;
		NewFuncAction->SectionID = NodeSectionID::GRAPH;
		OutAllActions.AddAction(NewFuncAction);

		GetChildGraphs(Graph, OutAllActions);
		GetChildEvents(Graph, NewFuncAction->SectionID, OutAllActions);
	}

	// Grab intermediate pages
	for (int32 i = 0; i < Blueprint->IntermediateGeneratedGraphs.Num(); i++)
	{
		UEdGraph* Graph = Blueprint->IntermediateGeneratedGraphs[i];
		check(Graph);
		
		const FName FunctionName(*(FString(TEXT("$INTERMEDIATE$_")) + Graph->GetName()));
		FString FunctionTooltip = FunctionName.ToString();
		FString FunctionDesc = FunctionName.ToString();
		TSharedPtr<FEdGraphSchemaAction_K2Graph> NewFuncAction = MakeShareable(new FEdGraphSchemaAction_K2Graph(EEdGraphSchemaAction_K2Graph::Graph, FString(), FText::FromString(FunctionDesc), FunctionTooltip, 1));
		NewFuncAction->FuncName = FunctionName;
		NewFuncAction->EdGraph = Graph;
		OutAllActions.AddAction(NewFuncAction);

		GetChildGraphs(Graph, OutAllActions);
		GetChildEvents(Graph, NewFuncAction->SectionID, OutAllActions);
	}
}

ECheckBoxState SMyBlueprint::OnUserVarsCheckState() const
{
	return !bShowUserVarsOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SMyBlueprint::OnUserVarsCheckStateChanged(ECheckBoxState InNewState)
{
	bShowUserVarsOnly = (InNewState != ECheckBoxState::Checked);
	Refresh();
}

FReply SMyBlueprint::OnActionDragged( const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, const FPointerEvent& MouseEvent )
{
	TSharedPtr<FEdGraphSchemaAction> InAction( InActions.Num() > 0 ? InActions[0] : NULL );
	if(InAction.IsValid())
	{
		auto AnalyticsDelegate = FNodeCreationAnalytic::CreateSP( this, &SMyBlueprint::UpdateNodeCreation );

		if(InAction->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Graph* FuncAction = (FEdGraphSchemaAction_K2Graph*)InAction.Get();
			
			if (FuncAction->GraphType == EEdGraphSchemaAction_K2Graph::Function ||FuncAction->GraphType == EEdGraphSchemaAction_K2Graph::Interface)
			{
				bool bIsBlueprintCallableFunction = false;
				if (FuncAction->EdGraph != NULL)
				{
					for (auto It = FuncAction->EdGraph->Nodes.CreateConstIterator(); It; ++It)
					{
						if (UK2Node_FunctionEntry* Node = Cast<UK2Node_FunctionEntry>(*It))
						{
							// See whether this node is a blueprint callable function
							if (Node->ExtraFlags & (FUNC_BlueprintCallable|FUNC_BlueprintPure))
							{
								bIsBlueprintCallableFunction = true;
							}
						}
					}
				}

				if (bIsBlueprintCallableFunction)
				{
					return FReply::Handled().BeginDragDrop(FKismetFunctionDragDropAction::New(FuncAction->FuncName, BlueprintEditorPtr.Pin()->GetBlueprintObj()->SkeletonGeneratedClass, FMemberReference(), AnalyticsDelegate));
				}
			}
			else if (FuncAction->GraphType == EEdGraphSchemaAction_K2Graph::Macro)
			{
				if ((FuncAction->EdGraph != NULL) && BlueprintEditorPtr.Pin()->GetBlueprintObj()->BlueprintType != BPTYPE_MacroLibrary)
				{
					return FReply::Handled().BeginDragDrop(FKismetMacroDragDropAction::New(FuncAction->FuncName, BlueprintEditorPtr.Pin()->GetBlueprintObj(), FuncAction->EdGraph, AnalyticsDelegate));
				}
			}
		}
		else if(InAction->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Delegate* DelegateAction = (FEdGraphSchemaAction_K2Delegate*)InAction.Get();
			check(DelegateAction->GetDelegateName() != NAME_None);
			UClass* VarClass = DelegateAction->GetDelegateClass();
			if(VarClass != NULL)
			{
				const bool bIsAltDown = MouseEvent.IsAltDown();
				const bool bIsCtrlDown = MouseEvent.IsLeftControlDown() || MouseEvent.IsRightControlDown();
				
				TSharedRef<FKismetVariableDragDropAction> DragOperation = FKismetDelegateDragDropAction::New(DelegateAction->GetDelegateName(), VarClass, AnalyticsDelegate);
				DragOperation->SetAltDrag(bIsAltDown);
				DragOperation->SetCtrlDrag(bIsCtrlDown);
				return FReply::Handled().BeginDragDrop(DragOperation);
			}
		}
		else if( InAction->GetTypeId() == FEdGraphSchemaAction_K2LocalVar::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2LocalVar* VarAction = (FEdGraphSchemaAction_K2LocalVar*)InAction.Get();

			UStruct* VariableScope = VarAction->GetVariableScope();
			if(VariableScope != NULL)
			{
				const bool bIsAltDown = MouseEvent.IsAltDown();
				const bool bIsCtrlDown = MouseEvent.IsLeftControlDown() || MouseEvent.IsRightControlDown();

				TSharedRef<FKismetVariableDragDropAction> DragOperation = FKismetVariableDragDropAction::New(VarAction->GetVariableName(), VariableScope, AnalyticsDelegate);
				DragOperation->SetAltDrag(bIsAltDown);
				DragOperation->SetCtrlDrag(bIsCtrlDown);
				return FReply::Handled().BeginDragDrop(DragOperation);
			}
		}
		else if(InAction->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)InAction.Get();

			UClass* VarClass = VarAction->GetVariableClass();
			if(VarClass != NULL)
			{
				const bool bIsAltDown = MouseEvent.IsAltDown();
				const bool bIsCtrlDown = MouseEvent.IsLeftControlDown() || MouseEvent.IsRightControlDown();
				
				TSharedRef<FKismetVariableDragDropAction> DragOperation = FKismetVariableDragDropAction::New(VarAction->GetVariableName(), VarClass, AnalyticsDelegate);
				DragOperation->SetAltDrag(bIsAltDown);
				DragOperation->SetCtrlDrag(bIsCtrlDown);
				return FReply::Handled().BeginDragDrop(DragOperation);
			}
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Event::StaticGetTypeId())
		{			
			// don't need a valid FCanBeDroppedDelegate because this entry means we already have this 
			// event placed (so this action will just focus it)
			TSharedRef<FKismetDragDropAction> DragOperation = FKismetDragDropAction::New(InAction, AnalyticsDelegate, FKismetDragDropAction::FCanBeDroppedDelegate());

			return FReply::Handled().BeginDragDrop(DragOperation);
		}
	}

	return FReply::Unhandled();
}

FReply SMyBlueprint::OnCategoryDragged(const FString& InCategory, const FPointerEvent& MouseEvent)
{
	TSharedRef<FMyBlueprintCategoryDragDropAction> DragOperation = FMyBlueprintCategoryDragDropAction::New(InCategory, SharedThis(this));
	return FReply::Handled().BeginDragDrop(DragOperation);
}

void SMyBlueprint::OnGlobalActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions)
{
	// If an action is being selected, clear the LocalGraphActionMenu of any selection it has, this keeps it so that only one menu will ever have selection at a time
	if(InActions.Num() && LocalGraphActionMenu.IsValid() && InActions.Num())
	{
		LocalGraphActionMenu->SelectItemByName(NAME_None);
	}
	OnActionSelected(InActions);
}

void SMyBlueprint::OnLocalActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions)
{
	// If an action is being selected, clear the GraphActionMenu of any selection it has, this keeps it so that only one menu will ever have selection at a time
	if(InActions.Num() && InActions.Num())
	{
		GraphActionMenu->SelectItemByName(NAME_None);
	}
	OnActionSelected(InActions);
}

void SMyBlueprint::OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions )
{
	TSharedPtr<FEdGraphSchemaAction> InAction(InActions.Num() > 0 ? InActions[0] : NULL);
	UBlueprint* CurrentBlueprint = Blueprint;
	TSharedPtr<SKismetInspector> CurrentInspector = Inspector.Pin();

	if( BlueprintEditorPtr.IsValid() )
	{
		if (BlueprintEditorPtr.Pin()->GetUISelectionState() == FBlueprintEditor::GraphPanel)
		{
			// clear graph panel selection
			BlueprintEditorPtr.Pin()->ClearSelectionInAllEditors();
		}
		BlueprintEditorPtr.Pin()->GetUISelectionState() = InAction.IsValid() ? FBlueprintEditor::MyBlueprint : FBlueprintEditor::NoSelection;
		CurrentBlueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();
		CurrentInspector = BlueprintEditorPtr.Pin()->GetInspector();
	}

	OnActionSelectedHelper( InAction, Blueprint, CurrentInspector.ToSharedRef() );
}

void SMyBlueprint::OnActionSelectedHelper(TSharedPtr<FEdGraphSchemaAction> InAction, UBlueprint* Blueprint, TSharedRef<SKismetInspector> Inspector)
{
	if(InAction.IsValid())
	{
		if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Graph* GraphAction = (FEdGraphSchemaAction_K2Graph*)InAction.Get();

			if (GraphAction->EdGraph)
			{
				FGraphDisplayInfo DisplayInfo;
				GraphAction->EdGraph->GetSchema()->GetGraphDisplayInformation(*GraphAction->EdGraph, DisplayInfo);
				Inspector->ShowDetailsForSingleObject(GraphAction->EdGraph, SKismetInspector::FShowDetailsOptions(DisplayInfo.PlainName.ToString()));
			}
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Delegate* DelegateAction = (FEdGraphSchemaAction_K2Delegate*)InAction.Get();
			if (UMulticastDelegateProperty* Property = DelegateAction->GetDelegatePoperty())
			{
				Inspector->ShowDetailsForSingleObject(Property, SKismetInspector::FShowDetailsOptions(Property->GetName()));
			}
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)InAction.Get();

			SKismetInspector::FShowDetailsOptions Options(VarAction->GetVariableName().ToString());

			Inspector->ShowDetailsForSingleObject(VarAction->GetProperty(), Options);
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2LocalVar::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2LocalVar* VarAction = (FEdGraphSchemaAction_K2LocalVar*)InAction.Get();

			SKismetInspector::FShowDetailsOptions Options(VarAction->GetVariableName().ToString());

			Inspector->ShowDetailsForSingleObject(VarAction->GetProperty(), Options);
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Enum::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Enum* EnumAction = (FEdGraphSchemaAction_K2Enum*)InAction.Get();

			SKismetInspector::FShowDetailsOptions Options(EnumAction->GetPathName().ToString());
			Options.bForceRefresh = true;

			Inspector->ShowDetailsForSingleObject(EnumAction->Enum, Options);
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Struct::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Struct* StructAction = (FEdGraphSchemaAction_K2Struct*)InAction.Get();

			SKismetInspector::FShowDetailsOptions Options(StructAction->GetPathName().ToString());
			Options.bForceRefresh = true;

			Inspector->ShowDetailsForSingleObject(StructAction->Struct, Options);
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2TargetNode::StaticGetTypeId() || InAction->GetTypeId() == FEdGraphSchemaAction_K2Event::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2TargetNode* TargetNodeAction = (FEdGraphSchemaAction_K2TargetNode*)InAction.Get();
			SKismetInspector::FShowDetailsOptions Options(TargetNodeAction->NodeTemplate->GetNodeTitle(ENodeTitleType::EditableTitle).ToString());
			Inspector->ShowDetailsForSingleObject(TargetNodeAction->NodeTemplate, Options);
		}
		else
		{
			Inspector->ShowDetailsForObjects(TArray<UObject*>());
		}
	}
	else
	{
		Inspector->ShowDetailsForObjects(TArray<UObject*>());
	}
}

void SMyBlueprint::OnActionDoubleClicked( const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions )
{
	if( !BlueprintEditorPtr.IsValid() )
	{
		return;
	}

	TSharedPtr<FEdGraphSchemaAction> InAction( InActions.Num() > 0 ? InActions[0] : NULL );

	UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();
	if(InAction.IsValid())
	{
		if(InAction->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Graph* GraphAction = (FEdGraphSchemaAction_K2Graph*)InAction.Get();

			if (GraphAction->EdGraph)
			{
				BlueprintEditorPtr.Pin()->OpenDocument(GraphAction->EdGraph, FDocumentTracker::OpenNewDocument);
			}
		}
		if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Delegate* DelegateAction = (FEdGraphSchemaAction_K2Delegate*)InAction.Get();

			if (DelegateAction->EdGraph)
			{
				BlueprintEditorPtr.Pin()->OpenDocument(DelegateAction->EdGraph, FDocumentTracker::OpenNewDocument);
			}
		}
		else if(InAction->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)InAction.Get();
			
			// timeline variables
			const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(VarAction->GetProperty());
			if (ObjectProperty &&
				ObjectProperty->PropertyClass &&
				ObjectProperty->PropertyClass->IsChildOf(UTimelineComponent::StaticClass()))
			{
				for (int32 i=0; i<Blueprint->Timelines.Num(); i++)
				{
					// Convert the Timeline's name to a variable name before comparing it to the variable
					if (FName(*UTimelineTemplate::TimelineTemplateNameToVariableName(Blueprint->Timelines[i]->GetFName())) == VarAction->GetVariableName())
					{
						BlueprintEditorPtr.Pin()->OpenDocument(Blueprint->Timelines[i], FDocumentTracker::OpenNewDocument);
					}
				}
			}
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2Event::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Event* EventNodeAction = (FEdGraphSchemaAction_K2Event*)InAction.Get();
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EventNodeAction->NodeTemplate);
		}
		else if (InAction->GetTypeId() == FEdGraphSchemaAction_K2TargetNode::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2TargetNode* TargetNodeAction = (FEdGraphSchemaAction_K2TargetNode*)InAction.Get();
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(TargetNodeAction->NodeTemplate);
		}
	}
}

template<class SchemaActionType> SchemaActionType* SelectionAsType( const TSharedPtr< SGraphActionMenu >& GraphActionMenu )
{
	TArray<TSharedPtr<FEdGraphSchemaAction> > SelectedActions;
	GraphActionMenu->GetSelectedActions(SelectedActions);

	SchemaActionType* Selection = NULL;

	TSharedPtr<FEdGraphSchemaAction> SelectedAction( SelectedActions.Num() > 0 ? SelectedActions[0] : NULL );
	if ( SelectedAction.IsValid() &&
		 SelectedAction->GetTypeId() == SchemaActionType::StaticGetTypeId() )
	{
		Selection = (SchemaActionType*)SelectedActions[0].Get();
	}

	return Selection;
}

FEdGraphSchemaAction_K2Enum* SMyBlueprint::SelectionAsEnum() const
{
	return SelectionAsType<FEdGraphSchemaAction_K2Enum>( GraphActionMenu );
}


FEdGraphSchemaAction_K2Struct* SMyBlueprint::SelectionAsStruct() const
{
	return SelectionAsType<FEdGraphSchemaAction_K2Struct>( GraphActionMenu );
}

FEdGraphSchemaAction_K2Graph* SMyBlueprint::SelectionAsGraph() const
{
	return SelectionAsType<FEdGraphSchemaAction_K2Graph>( GraphActionMenu );
}

FEdGraphSchemaAction_K2Var* SMyBlueprint::SelectionAsVar() const
{
	return SelectionAsType<FEdGraphSchemaAction_K2Var>( GraphActionMenu );
}

FEdGraphSchemaAction_K2LocalVar* SMyBlueprint::SelectionAsLocalVar() const
{
	if(LocalGraphActionMenu.IsValid())
	{
		return SelectionAsType<FEdGraphSchemaAction_K2LocalVar>( LocalGraphActionMenu );
	}
	return NULL;
}

FEdGraphSchemaAction_K2Delegate* SMyBlueprint::SelectionAsDelegate() const
{
	return SelectionAsType<FEdGraphSchemaAction_K2Delegate>( GraphActionMenu );
}

FEdGraphSchemaAction_K2Event* SMyBlueprint::SelectionAsEvent() const
{
	return SelectionAsType<FEdGraphSchemaAction_K2Event>( GraphActionMenu );
}

bool SMyBlueprint::SelectionIsCategory() const
{
	return !GraphActionMenu->GetSelectedCategoryName().IsEmpty();
}

bool SMyBlueprint::SelectionHasContextMenu() const
{
	bool bSelectionHasContextMenu = false;

	// Do not consider the selection having a context menu if the menu is being brought up in the other action menu, that being if an item is selected in the GraphActionMenu and you right click in LocalGraphActionMenu
	if(GraphActionMenu.IsValid() && GraphActionMenu->HasFocusedDescendants())
	{
		bSelectionHasContextMenu = SelectionAsGraph() || SelectionAsVar() || SelectionIsCategory() || SelectionAsDelegate() || SelectionAsEnum() || SelectionAsEvent() || SelectionAsStruct();
	}
	else if(LocalGraphActionMenu.IsValid() && LocalGraphActionMenu->HasFocusedDescendants())
	{
		bSelectionHasContextMenu = SelectionAsLocalVar() != NULL;
	}

	return bSelectionHasContextMenu;
}

FString SMyBlueprint::GetGraphCategory(UEdGraph* InGraph) const
{
	FString ReturnCategory;

	// Pull the category from the required metadata based on the types of nodes we can discover in the graph
	TWeakObjectPtr<UK2Node_EditablePinBase> EntryNode;
	TWeakObjectPtr<UK2Node_EditablePinBase> ResultNode;
	FBlueprintEditorUtils::GetEntryAndResultNodes(InGraph, EntryNode, ResultNode);
	if (UK2Node_FunctionEntry* FunctionEntryNode = Cast<UK2Node_FunctionEntry>(EntryNode.Get()))
	{
		ReturnCategory = FunctionEntryNode->MetaData.Category;
	}
	else if (UK2Node_Tunnel* TypedEntryNode = ExactCast<UK2Node_Tunnel>(EntryNode.Get()))
	{
		ReturnCategory = TypedEntryNode->MetaData.Category;
	}

	// Empty the category if it's default, we don't want to display the "default" category and items will just appear without a category
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if(ReturnCategory == K2Schema->VR_DefaultCategory.ToString())
	{
		ReturnCategory.Empty();
	}

	return ReturnCategory;
}

void SMyBlueprint::GetSelectedItemsForContextMenu(TArray<FComponentEventConstructionData>& OutSelectedItems) const
{
	FEdGraphSchemaAction_K2Var* Var = SelectionAsVar();
	if ( Var != NULL )
	{
		UObjectProperty* ComponentProperty = Cast<UObjectProperty>(Var->GetProperty());

		if ( ComponentProperty != NULL &&
			 ComponentProperty->PropertyClass != NULL &&
			 ComponentProperty->PropertyClass->IsChildOf( UActorComponent::StaticClass() ) )
		{
			FComponentEventConstructionData NewItem;
			NewItem.VariableName = Var->GetVariableName();
			NewItem.Component = Cast<UActorComponent>(ComponentProperty->PropertyClass->GetDefaultObject());

			OutSelectedItems.Add( NewItem );
		}
	}
}

TSharedPtr<SWidget> SMyBlueprint::OnContextMenuOpening()
{
	if( !BlueprintEditorPtr.IsValid() )
	{
		return TSharedPtr<SWidget>();
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection,BlueprintEditorPtr.Pin()->GetToolkitCommands());
	
	// Check if the selected action is valid for a context menu
	if (SelectionHasContextMenu())
	{
		MenuBuilder.BeginSection("BasicOperations");
		{
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().OpenGraph);
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().OpenGraphInNewTab);
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().FocusNode);
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().FocusNodeInNewTab);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("Rename", "Rename"), LOCTEXT("Rename_Tooltip", "Renames this function or variable from blueprint.") );
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().ImplementFunction);
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().FindEntry);
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().GotoNativeVarDefinition);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
			MenuBuilder.AddMenuEntry(FMyBlueprintCommands::Get().DeleteEntry);
		}
		MenuBuilder.EndSection();

		FEdGraphSchemaAction_K2Var* Var = SelectionAsVar();

		if ( Var && BlueprintEditorPtr.IsValid() && FBlueprintEditorUtils::DoesSupportEventGraphs(GetBlueprintObj()) )
		{
			UObjectProperty* ComponentProperty = Cast<UObjectProperty>(Var->GetProperty());

			if ( ComponentProperty && ComponentProperty->PropertyClass &&
				 ComponentProperty->PropertyClass->IsChildOf( UActorComponent::StaticClass() ) )
			{
				if( FBlueprintEditor::CanClassGenerateEvents( ComponentProperty->PropertyClass ))
				{
					TSharedPtr<FBlueprintEditor> BlueprintEditor(BlueprintEditorPtr.Pin());

					// If the selected item is valid, and is a component of some sort, build a context menu
					// of events appropriate to the component.
					MenuBuilder.AddSubMenu(	LOCTEXT("AddEventSubMenu", "Add Event"), 
											LOCTEXT("AddEventSubMenu_ToolTip", "Add Event"), 
											FNewMenuDelegate::CreateStatic(	&SSCSEditor::BuildMenuEventsSection, BlueprintEditor, ComponentProperty->PropertyClass, 
											FGetSelectedObjectsDelegate::CreateSP(this, &SMyBlueprint::GetSelectedItemsForContextMenu)));
				}
			}
		}
	}
	else
	{
		MenuBuilder.BeginSection("AddNewItem", LOCTEXT("AddOperations", "Add New"));
		{
			MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().AddNewVariable);
			MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().AddNewLocalVariable);
			MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().AddNewFunction);
			MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().AddNewMacroDeclaration);
			MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().AddNewEventGraph);
			MenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().AddNewDelegate);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

bool SMyBlueprint::CanOpenGraph() const
{
	const FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph();
	const bool bGraph = GraphAction && GraphAction->EdGraph;
	const FEdGraphSchemaAction_K2Delegate* DelegateAction = SelectionAsDelegate();
	const bool bDelegate = DelegateAction && DelegateAction->EdGraph;
	return (bGraph || bDelegate) && BlueprintEditorPtr.IsValid();
}

void SMyBlueprint::OpenGraph(FDocumentTracker::EOpenDocumentCause InCause)
{
	UEdGraph* EdGraph = NULL;

	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		EdGraph = GraphAction->EdGraph;
	}
	else if (FEdGraphSchemaAction_K2Delegate* DelegateAction = SelectionAsDelegate())
	{
		EdGraph = DelegateAction->EdGraph;
	}
	else if (FEdGraphSchemaAction_K2Event* EventAction = SelectionAsEvent())
	{
		EdGraph = EventAction->NodeTemplate->GetGraph();
	}
	
	if (EdGraph)
	{
		BlueprintEditorPtr.Pin()->OpenDocument(EdGraph, InCause);
	}
}


void SMyBlueprint::OnOpenGraph()
{
	OpenGraph(FDocumentTracker::OpenNewDocument);	
}

void SMyBlueprint::OnOpenGraphInNewTab()
{
	OpenGraph(FDocumentTracker::ForceOpenNewDocument);	
}

bool SMyBlueprint::CanFocusOnNode() const
{
	FEdGraphSchemaAction_K2Event const* const EventAction = SelectionAsEvent();
	return (EventAction != NULL) && (EventAction->NodeTemplate != NULL);
}

void SMyBlueprint::OnFocusNode()
{
	if (FEdGraphSchemaAction_K2Event* EventAction = SelectionAsEvent())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EventAction->NodeTemplate);
	}
}

void SMyBlueprint::OnFocusNodeInNewTab()
{
	OpenGraph(FDocumentTracker::ForceOpenNewDocument);
	OnFocusNode();
}

bool SMyBlueprint::CanImplementFunction() const
{
	FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph();
	return GraphAction && GraphAction->EdGraph == NULL;
}

void SMyBlueprint::OnImplementFunction()
{
	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		check(GetBlueprintObj()->SkeletonGeneratedClass);
		UFunction* OverrideFunc = FindField<UFunction>(GetBlueprintObj()->SkeletonGeneratedClass, GraphAction->FuncName);
		if (OverrideFunc == NULL)
		{
			// maybe it's from a native interface, check those too
			for ( UClass* TempClass=GetBlueprintObj()->ParentClass; (NULL != TempClass) && (NULL == OverrideFunc); TempClass=TempClass->GetSuperClass() )
			{
				for (int32 Idx=0; Idx<TempClass->Interfaces.Num(); ++Idx)
				{
					FImplementedInterface const& I = TempClass->Interfaces[Idx];
					if (!I.bImplementedByK2)
					{
						OverrideFunc = FindField<UFunction>(I.Class, GraphAction->FuncName);
						if (OverrideFunc)
						{
							// found it, done
							break;
						}
					}
				}
			}
		}
		check(OverrideFunc);
		UClass* const OverrideFuncClass = CastChecked<UClass>(OverrideFunc->GetOuter());

		// Implement the function graph
		UEdGraph* const NewGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), GraphAction->FuncName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph(GetBlueprintObj(), NewGraph, /*bIsUserCreated=*/ false, OverrideFuncClass);
		BlueprintEditorPtr.Pin()->OpenDocument(NewGraph, FDocumentTracker::OpenNewDocument);
	}
}

void SMyBlueprint::OnFindEntry()
{
	FString SearchTerm;
	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		SearchTerm = GraphAction->FuncName.ToString();
	}
	else if (FEdGraphSchemaAction_K2Var* VarAction = SelectionAsVar())
	{
		SearchTerm = VarAction->GetVariableName().ToString();
	}
	else if (FEdGraphSchemaAction_K2LocalVar* LocalVarAction = SelectionAsLocalVar())
	{
		SearchTerm = LocalVarAction->GetVariableName().ToString();
	}
	else if (FEdGraphSchemaAction_K2Delegate* DelegateAction = SelectionAsDelegate())
	{
		SearchTerm = DelegateAction->GetDelegateName().ToString();
	}
	else if (FEdGraphSchemaAction_K2Enum* EnumAction = SelectionAsEnum())
	{
		SearchTerm = EnumAction->Enum->GetName();
	}
	else if (FEdGraphSchemaAction_K2Struct* StructAction = SelectionAsStruct())
	{
		BlueprintEditorPtr.Pin()->SummonSearchUI(true, StructAction->Struct->GetName());
	}
	else if (FEdGraphSchemaAction_K2Event* EventAction = SelectionAsEvent())
	{
		SearchTerm = EventAction->MenuDescription.ToString();
	}

	if(!SearchTerm.IsEmpty())
	{
		BlueprintEditorPtr.Pin()->SummonSearchUI(true, FString::Printf(TEXT("\"%s\""), *SearchTerm));
	}
}

bool SMyBlueprint::CanFindEntry() const
{
	// Nothing relevant to the category will ever be found, unless the name of the category overlaps with another item
	if (SelectionIsCategory())
	{
		return false;
	}

	return true;
}

void SMyBlueprint::OnDeleteGraph(UEdGraph* InGraph, EEdGraphSchemaAction_K2Graph::Type InGraphType)
{
	if (InGraph)
	{
		const FScopedTransaction Transaction( LOCTEXT("RemoveGraph", "Remove Graph") );
		GetBlueprintObj()->Modify();

		InGraph->Modify();

		if (InGraphType == EEdGraphSchemaAction_K2Graph::Subgraph)
		{
			// Remove any composite nodes bound to this graph
			TArray<UK2Node_Composite*> AllCompositeNodes;
			FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Composite>(GetBlueprintObj(), AllCompositeNodes);

			const bool bDontRecompile = true;
			for (auto CompIt = AllCompositeNodes.CreateIterator(); CompIt; ++CompIt)
			{
				UK2Node_Composite* CompNode = *CompIt;
				if (CompNode->BoundGraph == InGraph)
				{
					FBlueprintEditorUtils::RemoveNode(GetBlueprintObj(), CompNode, bDontRecompile);
				}
			}
		}

		FBlueprintEditorUtils::RemoveGraph(GetBlueprintObj(), InGraph, EGraphRemoveFlags::Recompile);
		BlueprintEditorPtr.Pin()->CloseDocumentTab(InGraph);

		for (TObjectIterator<UK2Node_CreateDelegate> It(RF_ClassDefaultObject | RF_PendingKill); It; ++It)
		{
			if (It->GetGraph() != InGraph)
			{
				It->HandleAnyChange();
			}
		}

		InGraph = NULL;
	}
}

UEdGraph* SMyBlueprint::GetFocusedGraph() const
{
	auto BlueprintEditorPtrPinned = BlueprintEditorPtr.Pin();
	if( BlueprintEditorPtrPinned.IsValid() )
	{
		return BlueprintEditorPtrPinned->GetFocusedGraph();
	}

	return EdGraph;
}

void SMyBlueprint::OnDeleteDelegate(FEdGraphSchemaAction_K2Delegate* InDelegateAction)
{
	UEdGraph* EdGraph = InDelegateAction->EdGraph;
	UBlueprint* Blueprint = GetBlueprintObj();
	if (EdGraph && Blueprint)
	{
		const FScopedTransaction Transaction( LOCTEXT("RemoveDelegate", "Remove Event Dispatcher") );
		Blueprint->Modify();

		BlueprintEditorPtr.Pin()->CloseDocumentTab(EdGraph);
		EdGraph->Modify();

		FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, EdGraph->GetFName());
		FBlueprintEditorUtils::RemoveGraph(Blueprint, EdGraph, EGraphRemoveFlags::Recompile);

		for (TObjectIterator<UK2Node_CreateDelegate> It(RF_ClassDefaultObject | RF_PendingKill); It; ++It)
		{
			It->HandleAnyChange();
		}
	}
}

void SMyBlueprint::OnDeleteEntry()
{
	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		OnDeleteGraph(GraphAction->EdGraph, GraphAction->GraphType);
	}
	else if (FEdGraphSchemaAction_K2Delegate* DelegateAction = SelectionAsDelegate())
	{
		OnDeleteDelegate(DelegateAction);
	}
	else if ( FEdGraphSchemaAction_K2Var* VarAction = SelectionAsVar() )
	{
		if(FBlueprintEditorUtils::IsVariableUsed(GetBlueprintObj(), VarAction->GetVariableName()))
		{
			FText ConfirmDelete = FText::Format(LOCTEXT( "ConfirmDeleteVariableInUse",
				"Variable {0} is in use! Do you really want to delete it?"),
				FText::FromName( VarAction->GetVariableName() ) );

			// Warn the user that this may result in data loss
			FSuppressableWarningDialog::FSetupInfo Info( ConfirmDelete, LOCTEXT("DeleteVar", "Delete Variable"), "DeleteVariableInUse_Warning" );
			Info.ConfirmText = LOCTEXT( "DeleteVariable_Yes", "Yes");
			Info.CancelText = LOCTEXT( "DeleteVariable_No", "No");	

			FSuppressableWarningDialog DeleteVariableInUse( Info );
			if ( DeleteVariableInUse.ShowModal() == FSuppressableWarningDialog::Cancel )
			{
				return;
			}
		}

		const FScopedTransaction Transaction( LOCTEXT( "RemoveVariable", "Remove Variable" ) );

		GetBlueprintObj()->Modify();
		FBlueprintEditorUtils::RemoveMemberVariable(GetBlueprintObj(), VarAction->GetVariableName());
	}
	else if ( FEdGraphSchemaAction_K2LocalVar* LocalVarAction = SelectionAsLocalVar() )
	{
		if(FBlueprintEditorUtils::IsVariableUsed(GetBlueprintObj(), LocalVarAction->GetVariableName(), FBlueprintEditorUtils::FindScopeGraph(GetBlueprintObj(), LocalVarAction->GetVariableScope())))
		{
			FText ConfirmDelete = FText::Format(LOCTEXT( "ConfirmDeleteLocalVariableInUse",
				"Local Variable {0} is in use! Do you really want to delete it?"),
				FText::FromName( LocalVarAction->GetVariableName() ) );

			// Warn the user that this may result in data loss
			FSuppressableWarningDialog::FSetupInfo Info( ConfirmDelete, LOCTEXT("DeleteVar", "Delete Variable"), "DeleteVariableInUse_Warning" );
			Info.ConfirmText = LOCTEXT( "DeleteVariable_Yes", "Yes");
			Info.CancelText = LOCTEXT( "DeleteVariable_No", "No");	

			FSuppressableWarningDialog DeleteVariableInUse( Info );
			if ( DeleteVariableInUse.ShowModal() == FSuppressableWarningDialog::Cancel )
			{
				return;
			}
		}

		const FScopedTransaction Transaction( LOCTEXT( "RemoveLocalVariable", "Remove Local Variable" ) );

		GetBlueprintObj()->Modify();

		UEdGraph* FunctionGraph = FBlueprintEditorUtils::GetTopLevelGraph(GetFocusedGraph());
		TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
		FunctionGraph->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);
		check(FunctionEntryNodes.Num() == 1);
		FunctionEntryNodes[0]->Modify();

		FBlueprintEditorUtils::RemoveLocalVariable(GetBlueprintObj(), LocalVarAction->GetVariableScope(), LocalVarAction->GetVariableName());
	}
	else if (FEdGraphSchemaAction_K2Event* EventAction = SelectionAsEvent())
	{
		const FScopedTransaction Transaction(LOCTEXT( "RemoveEventNode", "Remove EventNode"));

		GetBlueprintObj()->Modify();
		FBlueprintEditorUtils::RemoveNode(GetBlueprintObj(), EventAction->NodeTemplate);
	}
	else if ( SelectionIsCategory() )
	{
		TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
		GraphActionMenu->GetSelectedCategorySubActions(Actions);
		if (Actions.Num())
		{
			FText TransactionTitle;

			switch((NodeSectionID::Type)Actions[0]->SectionID)
			{
			case NodeSectionID::VARIABLE:
			case NodeSectionID::LOCAL_VARIABLE:
				{
					TransactionTitle = LOCTEXT( "BulkRemoveVariables", "Bulk Remove Variables" );
					break;
				}
			case NodeSectionID::DELEGATE:
				{
					TransactionTitle = LOCTEXT( "BulkRemoveDelegates", "Bulk Remove Delegates" );
					break;
				}
			case NodeSectionID::FUNCTION:
				{
					TransactionTitle = LOCTEXT( "BulkRemoveFunctions", "Bulk Remove Functions" );
					break;
				}
			case NodeSectionID::MACRO:
				{
					TransactionTitle = LOCTEXT( "BulkRemoveMacros", "Bulk Remove Macros" );
					break;
				}
			default:
				{
					TransactionTitle = LOCTEXT( "BulkRemove", "Bulk Remove Items" );
				}
			}

			const FScopedTransaction Transaction( TransactionTitle);

			GetBlueprintObj()->Modify();
			for (int32 i = 0; i < Actions.Num(); ++i)
			{
				if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
				{
					FEdGraphSchemaAction_K2Var* Var = (FEdGraphSchemaAction_K2Var*)Actions[i].Get();
					
					FBlueprintEditorUtils::RemoveMemberVariable(GetBlueprintObj(), Var->GetVariableName());
				}
				else if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2LocalVar::StaticGetTypeId())
				{
					FEdGraphSchemaAction_K2LocalVar* K2LocalVarAction = (FEdGraphSchemaAction_K2LocalVar*)Actions[i].Get();

					FBlueprintEditorUtils::RemoveLocalVariable(GetBlueprintObj(), K2LocalVarAction->GetVariableScope(), K2LocalVarAction->GetVariableName());
				}
				else if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
				{
					FEdGraphSchemaAction_K2Graph* K2GraphAction = (FEdGraphSchemaAction_K2Graph*)Actions[i].Get();

					OnDeleteGraph(K2GraphAction->EdGraph, K2GraphAction->GraphType);
				}
				else if (Actions[i]->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
				{
					OnDeleteDelegate((FEdGraphSchemaAction_K2Delegate*)Actions[i].Get());
				}
			}
		}
	}

	Refresh();
	BlueprintEditorPtr.Pin()->GetInspector()->ShowDetailsForObjects(TArray<UObject*>());
}

struct FDeleteEntryHelper
{
	static bool CanDeleteVariable(const UBlueprint* Blueprint, FName VarName)
	{
		check(NULL != Blueprint);

		const UProperty* VariableProperty = FindField<UProperty>(Blueprint->SkeletonGeneratedClass, VarName);
		const UClass* VarSourceClass = CastChecked<const UClass>(VariableProperty->GetOuter());
		const bool bIsBlueprintVariable = (VarSourceClass == Blueprint->SkeletonGeneratedClass);
		const int32 VarInfoIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableProperty->GetFName());
		const bool bHasVarInfo = (VarInfoIndex != INDEX_NONE);

		return bIsBlueprintVariable && bHasVarInfo;
	}
};

bool SMyBlueprint::CanDeleteEntry() const
{
	// Cannot delete entries while not in editing mode
	if(!BlueprintEditorPtr.Pin()->InEditingMode())
	{
		return false;
	}

	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		return GraphAction->EdGraph != NULL && GraphAction->EdGraph->bAllowDeletion;
	}
	else if (FEdGraphSchemaAction_K2Delegate* DelegateAction = SelectionAsDelegate())
	{
		return (DelegateAction->EdGraph != NULL) && (DelegateAction->EdGraph->bAllowDeletion) && 
			FDeleteEntryHelper::CanDeleteVariable(GetBlueprintObj(), DelegateAction->GetDelegateName());
	}
	else if (FEdGraphSchemaAction_K2Var* VarAction = SelectionAsVar())
	{
		return FDeleteEntryHelper::CanDeleteVariable(GetBlueprintObj(), VarAction->GetVariableName());
	}
	else if (FEdGraphSchemaAction_K2Event* EventAction = SelectionAsEvent())
	{
		return EventAction->NodeTemplate != NULL;
	}
	else if (FEdGraphSchemaAction_K2LocalVar* LocalVariable = SelectionAsLocalVar())
	{
		return true;
	}
	else if (SelectionIsCategory())
	{
		// Can't delete categories if they can't be renamed, that means they are native
		if(GraphActionMenu->CanRequestRenameOnActionNode())
		{
			return true;
		}
	}
	return false;
}

bool SMyBlueprint::IsDuplicateActionVisible() const
{
	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		// Functions in interface Blueprints cannot be duplicated
		if(GetBlueprintObj()->BlueprintType != BPTYPE_Interface)
		{
			// Only display it for valid function graphs
			return GraphAction->EdGraph && GraphAction->EdGraph->GetSchema()->CanDuplicateGraph(GraphAction->EdGraph);
		}
	}
	else if (SelectionAsVar() || SelectionAsLocalVar())
	{
		return true;
	}
	return false;
}

bool SMyBlueprint::CanDuplicateAction() const
{
	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		// Only support function graph duplication
		if(GraphAction->EdGraph && GraphAction->EdGraph->GetSchema()->GetGraphType(GraphAction->EdGraph) == GT_Function)
		{
			return GraphAction->EdGraph->GetSchema()->CanDuplicateGraph(GraphAction->EdGraph);
		}
	}
	else if(SelectionAsVar() || SelectionAsLocalVar())
	{
		return true;
	}
	return false;
}

void SMyBlueprint::OnDuplicateAction()
{
	FName DuplicateActionName = NAME_None;

	if (FEdGraphSchemaAction_K2Graph* GraphAction = SelectionAsGraph())
	{
		const FScopedTransaction Transaction( LOCTEXT( "DuplicateGraph", "Duplicate Graph" ) );
		GetBlueprintObj()->Modify();

		UEdGraph* DuplicatedGraph = GraphAction->EdGraph->GetSchema()->DuplicateGraph(GraphAction->EdGraph);
		check(DuplicatedGraph);

		DuplicatedGraph->Modify();

		// Only function duplication is supported
		EGraphType GraphType = DuplicatedGraph->GetSchema()->GetGraphType(GraphAction->EdGraph);
		check(GraphType == GT_Function);

		GetBlueprintObj()->FunctionGraphs.Add(DuplicatedGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());

		DuplicateActionName = DuplicatedGraph->GetFName();
	}
	else if (FEdGraphSchemaAction_K2Var* VarAction = SelectionAsVar())
	{
		const FScopedTransaction Transaction( LOCTEXT( "DuplicateVariable", "Duplicate Variable" ) );
		GetBlueprintObj()->Modify();

		if(FBlueprintEditorUtils::FindNewVariableIndex(GetBlueprintObj(), VarAction->GetVariableName()) != INDEX_NONE)
		{
			DuplicateActionName = FBlueprintEditorUtils::DuplicateVariable(GetBlueprintObj(), nullptr, VarAction->GetVariableName());
		}
		else
		{
			FEdGraphPinType VarPinType;
			GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(VarAction->GetProperty(), VarPinType);
			FBlueprintEditorUtils::AddMemberVariable(GetBlueprintObj(), FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, VarAction->GetVariableName().ToString()), VarPinType);
		}
	}
	else if (FEdGraphSchemaAction_K2LocalVar* LocalVarAction = SelectionAsLocalVar())
	{
		const FScopedTransaction Transaction( LOCTEXT( "Duplicate Local Variable", "Duplicate Local Variable" ) );
		GetBlueprintObj()->Modify();

		DuplicateActionName = FBlueprintEditorUtils::DuplicateVariable(GetBlueprintObj(), LocalVarAction->GetVariableScope(), LocalVarAction->GetVariableName());
	}

	// Select and rename the duplicated action
	if(DuplicateActionName != NAME_None)
	{
		SelectItemByName(DuplicateActionName);
		OnRequestRenameOnActionNode();
	}
}

void SMyBlueprint::GotoNativeCodeVarDefinition()
{
	if( FEdGraphSchemaAction_K2Var* VarAction = SelectionAsVar() )
	{
		if( UProperty* VarProperty = VarAction->GetProperty() )
		{
			FSourceCodeNavigation::NavigateToProperty( VarProperty );
		}
	}
}

bool SMyBlueprint::IsNativeVariable() const
{
	if( FEdGraphSchemaAction_K2Var* VarAction = SelectionAsVar() )
	{
		UProperty* VarProperty = VarAction->GetProperty();

		if( VarProperty && VarProperty->HasAllFlags( RF_Native ))
		{
			return true;
		}
	}
	return false;
}

void SMyBlueprint::OnResetItemFilter()
{
	FilterBox->SetText(FText::GetEmpty());
}

void SMyBlueprint::EnsureLastPinTypeValid()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const bool bLastPinTypeValid = (Schema->PC_Struct != LastPinType.PinCategory) || LastPinType.PinSubCategoryObject.IsValid();
	const bool bLastFunctionPinTypeValid = (Schema->PC_Struct != LastFunctionPinType.PinCategory) || LastFunctionPinType.PinSubCategoryObject.IsValid();
	if (!bLastPinTypeValid || !bLastFunctionPinTypeValid)
	{
		ResetLastPinType();
	}
}

void SMyBlueprint::ResetLastPinType()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	LastPinType.PinCategory = Schema->PC_Boolean;
	LastPinType.PinSubCategory = TEXT("");
	LastPinType.bIsArray = false;
	LastPinType.bIsReference = false;
	LastFunctionPinType = LastPinType;
}

void SMyBlueprint::UpdateNodeCreation()
{
	if( BlueprintEditorPtr.IsValid() )
	{
		BlueprintEditorPtr.Pin()->UpdateNodeCreationStats( ENodeCreateAction::MyBlueprintDragPlacement );
	}
}

FReply SMyBlueprint::OnAddNewLocalVariable()
{
	if( BlueprintEditorPtr.IsValid() )
	{
		BlueprintEditorPtr.Pin()->OnAddNewLocalVariable();
	}

	return FReply::Handled();
}

void SMyBlueprint::OnFilterTextChanged( const FText& InFilterText )
{
	GraphActionMenu->GenerateFilteredItems(false);
	if (LocalGraphActionMenu.IsValid())
	{
		LocalGraphActionMenu->GenerateFilteredItems(false);
	}
}

FText SMyBlueprint::GetFilterText() const
{
	return FilterBox->GetText();
}

void SMyBlueprint::OnRequestRenameOnActionNode()
{
	// Attempt to rename in both menus, only one of them will have anything selected
	GraphActionMenu->OnRequestRenameOnActionNode();
	if (LocalGraphActionMenu.IsValid())
	{
		LocalGraphActionMenu->OnRequestRenameOnActionNode();
	}
}

bool SMyBlueprint::CanRequestRenameOnActionNode() const
{
	TArray<TSharedPtr<FEdGraphSchemaAction> > SelectedActions;
	GraphActionMenu->GetSelectedActions(SelectedActions);

	// If there is anything selected in the GraphActionMenu, check the item for if it can be renamed.
	if(SelectedActions.Num())
	{
		return GraphActionMenu->CanRequestRenameOnActionNode();
	}
	else if(LocalGraphActionMenu.IsValid())
	{
		return LocalGraphActionMenu->CanRequestRenameOnActionNode();
	}
	return false;
}

void SMyBlueprint::SelectItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo, int32 SectionId/* = INDEX_NONE*/, bool bIsCategory/* = false*/)
{
	// Check if the graph action menu is being told to clear
	if(ItemName == NAME_None)
	{
		ClearGraphActionMenuSelection();
	}
	// Attempt to select the item in the main graph action menu, if that fails, attempt the same in LocalGraphActionMenu
	else if( !GraphActionMenu->SelectItemByName(ItemName, SelectInfo, SectionId, bIsCategory) && LocalGraphActionMenu.IsValid() )
	{
		LocalGraphActionMenu->SelectItemByName(ItemName, SelectInfo, SectionId, bIsCategory);
	}
}

void SMyBlueprint::ClearGraphActionMenuSelection()
{
	GraphActionMenu->SelectItemByName(NAME_None);
	if (LocalGraphActionMenu.IsValid())
	{
		LocalGraphActionMenu->SelectItemByName(NAME_None);
	}
}

void SMyBlueprint::ExpandCategory(const FString& CategoryName)
{
	GraphActionMenu->ExpandCategory(CategoryName);
	if (LocalGraphActionMenu.IsValid())
	{
		LocalGraphActionMenu->ExpandCategory(CategoryName);
	}
}

bool SMyBlueprint::MoveCategoryBeforeCategory( const FString& InCategoryToMove, const FString& InTargetCategory )
{
	bool bResult = false;
	UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();

	if( Blueprint )
	{
		// Find root categories
		int32 RootCategoryDelim = InCategoryToMove.Find( TEXT( "|" ));
		FName CategoryToMove = RootCategoryDelim == INDEX_NONE ? *InCategoryToMove : *InCategoryToMove.Left( RootCategoryDelim );
		RootCategoryDelim = InTargetCategory.Find( TEXT( "|" ));
		FName TargetCategory = RootCategoryDelim == INDEX_NONE ? *InTargetCategory : *InTargetCategory.Left( RootCategoryDelim );

		TArray<FName>& CategorySort = Blueprint->CategorySorting;
		const int32 RemovalIndex = CategorySort.Find( CategoryToMove );
		// Remove existing sort index
		if( RemovalIndex != INDEX_NONE )
		{
			CategorySort.RemoveAt( RemovalIndex );
		}
		// Update the Category sort order and refresh ( if the target category has an entry )
		const int32 InsertIndex = CategorySort.Find( TargetCategory );
		if( InsertIndex != INDEX_NONE )
		{
			CategorySort.Insert( CategoryToMove, InsertIndex );
			Refresh();
			bResult = true;
		}
	}

	return bResult;
}

#undef LOCTEXT_NAMESPACE
