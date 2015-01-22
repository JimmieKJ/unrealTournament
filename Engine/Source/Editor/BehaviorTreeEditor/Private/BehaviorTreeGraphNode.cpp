// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "ScopedTransaction.h"
#include "SGraphEditorActionMenu_BehaviorTree.h"
#include "BlueprintNodeHelpers.h"
#include "BehaviorTree/BehaviorTree.h"
#include "AssetData.h"
#include "BehaviorTree/Decorators/BTDecorator_BlueprintBase.h"
#include "BehaviorTree/Services/BTService_BlueprintBase.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeGraphNode"

UBehaviorTreeGraphNode::UBehaviorTreeGraphNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeInstance = NULL;
	bHighlightInAbortRange0 = false;
	bHighlightInAbortRange1 = false;
	bHighlightInSearchRange0 = false;
	bHighlightInSearchRange1 = false;
	bHighlightInSearchTree = false;
	bHighlightChildNodeIndices = false;
	bRootLevel = false;
	bInjectedNode = false;
	bHasObserverError = false;
	bHasBreakpoint = false;
	bIsBreakpointEnabled = false;
	bDebuggerMarkCurrentlyActive = false;
	bDebuggerMarkPreviouslyActive = false;
	bDebuggerMarkFlashActive = false;
	bDebuggerMarkSearchSucceeded = false;
	bDebuggerMarkSearchFailed = false;
	bDebuggerMarkSearchTrigger = false;
	bDebuggerMarkSearchFailedTrigger = false;
	DebuggerSearchPathIndex = -1;
	DebuggerSearchPathSize = 0;
	DebuggerUpdateCounter = -1;
}

void UBehaviorTreeGraphNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UBehaviorTreeEditorTypes::PinCategory_MultipleNodes, TEXT(""), NULL, false, false, TEXT("In"));
	CreatePin(EGPD_Output, UBehaviorTreeEditorTypes::PinCategory_MultipleNodes, TEXT(""), NULL, false, false, TEXT("Out"));
}

void UBehaviorTreeGraphNode::PostPlacedNewNode()
{
	UClass* NodeClass = ClassData.GetClass(true);
	if (NodeClass)
	{
		UBehaviorTree* BT = Cast<UBehaviorTree>(GetBehaviorTreeGraph()->GetOuter());
		if (BT)
		{
			NodeInstance = ConstructObject<UBTNode>(NodeClass, BT);

			UBTNode* BTNode = (UBTNode*)NodeInstance;
			BTNode->SetFlags(RF_Transactional);
			BTNode->InitializeFromAsset(*BT);
			BTNode->InitializeNode(NULL, MAX_uint16, 0, 0);
		}
	}
}

bool UBehaviorTreeGraphNode::CanDuplicateNode() const
{
	return bInjectedNode ? false : Super::CanDuplicateNode();
}

bool UBehaviorTreeGraphNode::CanUserDeleteNode() const
{
	return bInjectedNode ? false : Super::CanUserDeleteNode();
}

void UBehaviorTreeGraphNode::PrepareForCopying()
{
	if (NodeInstance)
	{
		// Temporarily take ownership of the node instance, so that it is not deleted when cutting
		NodeInstance->Rename(NULL, this, REN_DontCreateRedirectors | REN_DoNotDirty );
	}
}
#if WITH_EDITOR

void UBehaviorTreeGraphNode::PostEditImport()
{
	ResetNodeOwner();

	if (NodeInstance)
	{
		UBehaviorTree* BT = Cast<UBehaviorTree>(GetBehaviorTreeGraph()->GetOuter());
		if (BT)
		{
			UBTNode* BTNode = (UBTNode*)NodeInstance;
			BTNode->InitializeFromAsset(*BT);
			BTNode->InitializeNode(NULL, MAX_uint16, 0, 0);
		}
	}
}

void UBehaviorTreeGraphNode::PostEditUndo()
{
	ResetNodeOwner();
}

#endif

void UBehaviorTreeGraphNode::PostCopyNode()
{
	ResetNodeOwner();
}

void UBehaviorTreeGraphNode::ResetNodeOwner()
{
	if (NodeInstance)
	{
		UBehaviorTree* BT = Cast<UBehaviorTree>(GetBehaviorTreeGraph()->GetOuter());
		NodeInstance->Rename(NULL, BT, REN_DontCreateRedirectors | REN_DoNotDirty);
		NodeInstance->ClearFlags(RF_Transient);

		// also reset decorators and services
		for(auto& Decorator : Decorators)
		{
			if(Decorator->NodeInstance != nullptr)
			{
				Decorator->NodeInstance->Rename(NULL, BT, REN_DontCreateRedirectors | REN_DoNotDirty);
				Decorator->NodeInstance->ClearFlags(RF_Transient);
			}
		}

		for(auto& Service : Services)
		{
			if(Service->NodeInstance != nullptr)
			{
				Service->NodeInstance->Rename(NULL, BT, REN_DontCreateRedirectors | REN_DoNotDirty);
				Service->NodeInstance->ClearFlags(RF_Transient);
			}
		}
	}
}

FString	UBehaviorTreeGraphNode::GetDescription() const
{
	if (const UBTNode* Node = Cast<UBTNode>(NodeInstance))
	{
		return Node->GetStaticDescription();
	}

	FString StoredClassName = ClassData.GetClassName();
	StoredClassName.RemoveFromEnd(TEXT("_C"));

	return FString::Printf(TEXT("%s: %s"), *StoredClassName,
		*LOCTEXT("NodeClassError", "Class not found, make sure it's saved!").ToString());
}

FText UBehaviorTreeGraphNode::GetTooltipText() const
{
	FText TooltipDesc;

	if(!NodeInstance)
	{
		TooltipDesc = LOCTEXT("NodeClassError", "Class not found, make sure it's saved!");
	}
	else
	{
		if(bHasObserverError)
		{
			TooltipDesc = LOCTEXT("ObserverError", "Observer has invalid abort setting!");
		}
		else if(DebuggerRuntimeDescription.Len() > 0)
		{
			TooltipDesc = FText::FromString(DebuggerRuntimeDescription);
		}
		else if(ErrorMessage.Len() > 0)
		{
			TooltipDesc = FText::FromString(ErrorMessage);
		}
		else
		{
			if( NodeInstance->GetClass()->IsChildOf(UBTDecorator_BlueprintBase::StaticClass()) ||
				NodeInstance->GetClass()->IsChildOf(UBTService_BlueprintBase::StaticClass()) ||
				NodeInstance->GetClass()->IsChildOf(UBTTask_BlueprintBase::StaticClass()) )
			{
				FAssetData AssetData(NodeInstance->GetClass()->ClassGeneratedBy);
				const FString* Description = AssetData.TagsAndValues.Find(GET_MEMBER_NAME_CHECKED(UBlueprint, BlueprintDescription));

				if (Description && !Description->IsEmpty())
				{
					TooltipDesc = FText::FromString(Description->Replace(TEXT("\\n"), TEXT("\n")));
				}
			}
			else
			{
				TooltipDesc = NodeInstance->GetClass()->GetToolTipText();
			}	
		}
	}

	if (bInjectedNode)
	{
		FText const InjectedDesc = !TooltipDesc.IsEmpty() ? TooltipDesc : FText::FromString(GetDescription());
		// @TODO: FText::Format() is slow... consider caching this tooltip like 
		//        we do for a lot of the BP nodes now (unfamiliar with this 
		//        node's purpose, so hesitant to muck with this at this time).
		TooltipDesc = FText::Format(LOCTEXT("InjectedTooltip", "Injected: {0}"), InjectedDesc);
	}

	return TooltipDesc;
}

UEdGraphPin* UBehaviorTreeGraphNode::GetInputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return NULL;
}

UEdGraphPin* UBehaviorTreeGraphNode::GetOutputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return NULL;
}

void UBehaviorTreeGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != nullptr)
	{
		UEdGraphPin* OutputPin = GetOutputPin();

		if (GetSchema()->TryCreateConnection(FromPin, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
		else if(OutputPin != nullptr && GetSchema()->TryCreateConnection(OutputPin, FromPin))
		{
			NodeConnectionListChanged();
		}
	}
}


UBehaviorTreeGraph* UBehaviorTreeGraphNode::GetBehaviorTreeGraph()
{
	return CastChecked<UBehaviorTreeGraph>(GetGraph());
}

void UBehaviorTreeGraphNode::NodeConnectionListChanged()
{
	GetBehaviorTreeGraph()->UpdateAsset(UBehaviorTreeGraph::SkipDebuggerFlags);
}


bool UBehaviorTreeGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UEdGraphSchema_BehaviorTree::StaticClass());
}

void UBehaviorTreeGraphNode::DiffProperties(UStruct* Struct, void* DataA, void* DataB, FDiffResults& Results, FDiffSingleResult& Diff)
{
	for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Prop = *PropertyIt;
 		// skip properties we cant see
		if (!Prop->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible) ||
			Prop->HasAnyPropertyFlags(CPF_Transient) ||
			Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance) ||
			Prop->IsA(UFunction::StaticClass()) ||
			Prop->IsA(UDelegateProperty::StaticClass()) ||
			Prop->IsA(UMulticastDelegateProperty::StaticClass()))
		{
			continue;
		}
		
		FString ValueStringA = BlueprintNodeHelpers::DescribeProperty(Prop, Prop->ContainerPtrToValuePtr<uint8>(DataA));
		FString ValueStringB  = BlueprintNodeHelpers::DescribeProperty(Prop, Prop->ContainerPtrToValuePtr<uint8>(DataB));

		if ( ValueStringA != ValueStringB )
		{
			if(Results)
			{
				Diff.DisplayString = FString::Printf(*LOCTEXT("DIF_NodeProperty", "Property Changed: %s ").ToString(), *Prop->GetName());
				Results.Add(Diff);
			}
		}
	}
}

void UBehaviorTreeGraphNode::FindDiffs(UEdGraphNode* OtherNode, FDiffResults& Results)
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::NODE_PROPERTY;
	Diff.Node1 = this;
	Diff.Node2 = OtherNode;
	Diff.ToolTip =  FString::Printf(*LOCTEXT("DIF_NodePropertyToolTip", "A Property of the node has changed").ToString());
	Diff.DisplayColor = FLinearColor(0.25f,0.71f,0.85f);
	
	UBehaviorTreeGraphNode* ThisBehaviorTreeNode = Cast<UBehaviorTreeGraphNode>(this);
	UBehaviorTreeGraphNode* OtherBehaviorTreeNode = Cast<UBehaviorTreeGraphNode>(OtherNode);
	if(ThisBehaviorTreeNode && OtherBehaviorTreeNode)
	{
		DiffProperties( ThisBehaviorTreeNode->GetClass(), ThisBehaviorTreeNode, OtherBehaviorTreeNode, Results, Diff );

		UBTNode* ThisNodeInstance = Cast<UBTNode>(ThisBehaviorTreeNode->NodeInstance);
	    UBTNode* OtherNodeInstance = Cast<UBTNode>(OtherBehaviorTreeNode->NodeInstance);
	    if(ThisNodeInstance && OtherNodeInstance)
	    {
		    DiffProperties( ThisNodeInstance->GetClass(), ThisNodeInstance, OtherNodeInstance, Results, Diff );
		}
	}
}

void UBehaviorTreeGraphNode::AddSubNode(UBehaviorTreeGraphNode* NodeTemplate, class UEdGraph* ParentGraph)
{
	const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
	ParentGraph->Modify();
	Modify();

	NodeTemplate->SetFlags(RF_Transactional);

	// set outer to be the graph so it doesn't go away
	NodeTemplate->Rename(NULL, ParentGraph, REN_NonTransactional);
	NodeTemplate->ParentNode = this;

	NodeTemplate->CreateNewGuid();
	NodeTemplate->PostPlacedNewNode();
	NodeTemplate->AllocateDefaultPins();
	NodeTemplate->AutowireNewNode(NULL);

	NodeTemplate->NodePosX = 0;
	NodeTemplate->NodePosY = 0;

	if (Cast<UBehaviorTreeGraphNode_CompositeDecorator>(NodeTemplate) || Cast<UBehaviorTreeGraphNode_Decorator>(NodeTemplate))
	{
		bool bAppend = true;
		for (int32 Idx = 0; Idx < Decorators.Num(); Idx++)
		{
			if (Decorators[Idx]->bInjectedNode)
			{
				Decorators.Insert(NodeTemplate, Idx);
				bAppend = false;
				break;
			}
		}

		if (bAppend)
		{
			Decorators.Add(NodeTemplate);
		}
	} 
	else
	{
		Services.Add(NodeTemplate);
	}

	ParentGraph->NotifyGraphChanged();
	GetBehaviorTreeGraph()->UpdateAsset(UBehaviorTreeGraph::SkipDebuggerFlags);
}


FString GetShortTypeNameHelper(const UObject* Ob)
{
	if (Ob->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return Ob->GetClass()->GetName().LeftChop(2);
	}

	FString TypeDesc = Ob->GetClass()->GetName();
	const int32 ShortNameIdx = TypeDesc.Find(TEXT("_"));
	if (ShortNameIdx != INDEX_NONE)
	{
		TypeDesc = TypeDesc.Mid(ShortNameIdx + 1);
	}

	return TypeDesc;
}

void UBehaviorTreeGraphNode::CreateAddDecoratorSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const
{
	TSharedRef<SGraphEditorActionMenu_BehaviorTree> Menu =	
		SNew(SGraphEditorActionMenu_BehaviorTree)
		.GraphObj( Graph )
		.GraphNode((UBehaviorTreeGraphNode*)this)
		.SubNodeType(ESubNode::Decorator)
		.AutoExpandActionMenu(true);

	MenuBuilder.AddWidget(Menu,FText(),true);
}

void UBehaviorTreeGraphNode::CreateAddServiceSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const
{
	TSharedRef<SGraphEditorActionMenu_BehaviorTree> Menu =	
		SNew(SGraphEditorActionMenu_BehaviorTree)
		.GraphObj( Graph )
		.GraphNode((UBehaviorTreeGraphNode*)this)
		.SubNodeType(ESubNode::Service)
		.AutoExpandActionMenu(true);

	MenuBuilder.AddWidget(Menu,FText(),true);
}

void UBehaviorTreeGraphNode::AddContextMenuActionsDecorators(const FGraphNodeContextMenuBuilder& Context) const
{
	Context.MenuBuilder->AddSubMenu(
		LOCTEXT("AddDecorator", "Add Decorator..." ),
		LOCTEXT("AddDecoratorTooltip", "Adds new decorator as a subnode" ),
		FNewMenuDelegate::CreateUObject( this, &UBehaviorTreeGraphNode::CreateAddDecoratorSubMenu,(UEdGraph*)Context.Graph));
}

void UBehaviorTreeGraphNode::AddContextMenuActionsServices(const FGraphNodeContextMenuBuilder& Context) const
{
	Context.MenuBuilder->AddSubMenu(
		LOCTEXT("AddService", "Add Service..." ),
		LOCTEXT("AddServiceTooltip", "Adds new service as a subnode" ),
		FNewMenuDelegate::CreateUObject( this, &UBehaviorTreeGraphNode::CreateAddServiceSubMenu,(UEdGraph*)Context.Graph));
}

void UBehaviorTreeGraphNode::DestroyNode()
{
	if (ParentNode)
	{
		ParentNode->Modify();
		ParentNode->Decorators.Remove(this);
		ParentNode->Services.Remove(this);
	}
	UEdGraphNode::DestroyNode();
}

void UBehaviorTreeGraphNode::ClearDebuggerState()
{
	bHasBreakpoint = false;
	bIsBreakpointEnabled = false;
	bDebuggerMarkCurrentlyActive = false;
	bDebuggerMarkPreviouslyActive = false;
	bDebuggerMarkFlashActive = false;
	bDebuggerMarkSearchSucceeded = false;
	bDebuggerMarkSearchFailed = false;
	bDebuggerMarkSearchTrigger = false;
	bDebuggerMarkSearchFailedTrigger = false;
	DebuggerSearchPathIndex = -1;
	DebuggerSearchPathSize = 0;
	DebuggerUpdateCounter = -1;
	DebuggerRuntimeDescription.Empty();
}

FName UBehaviorTreeGraphNode::GetNameIcon() const
{
	UBTNode* BTNodeInstance = Cast<UBTNode>(NodeInstance);
	return BTNodeInstance != nullptr ? BTNodeInstance->GetNodeIconName() : FName("BTEditor.Graph.BTNode.Icon");
}

bool UBehaviorTreeGraphNode::UsesBlueprint() const
{
	UBTNode* BTNodeInstance = Cast<UBTNode>(NodeInstance);
	return BTNodeInstance != nullptr ? BTNodeInstance->UsesBlueprint() : false;
}

bool UBehaviorTreeGraphNode::RefreshNodeClass()
{
	bool bUpdated = false;
	if (NodeInstance == NULL)
	{
		if (FClassBrowseHelper::IsClassKnown(ClassData))
		{
			PostPlacedNewNode();
			bUpdated = (NodeInstance != NULL);
		}
		else
		{
			FClassBrowseHelper::AddUnknownClass(ClassData);
		}
	}

	return bUpdated;
}

bool UBehaviorTreeGraphNode::HasErrors() const
{
	return bHasObserverError || ErrorMessage.Len() > 0 || NodeInstance == NULL;
}


#undef LOCTEXT_NAMESPACE
