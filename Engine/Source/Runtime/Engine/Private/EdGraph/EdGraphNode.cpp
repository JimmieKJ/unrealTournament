// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EdGraph/EdGraph.h"
#include "BlueprintUtilities.h"
#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "SlateBasics.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/Kismet2/Kismet2NameValidators.h"
#include "Editor/Kismet/Public/FindInBlueprintManager.h"
#endif

#define LOCTEXT_NAMESPACE "EdGraph"

FName const FNodeMetadata::DefaultGraphNode(TEXT("DefaultGraphNode"));

/////////////////////////////////////////////////////
// FGraphNodeContextMenuBuilder

FGraphNodeContextMenuBuilder::FGraphNodeContextMenuBuilder(const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, FMenuBuilder* InMenuBuilder, bool bInDebuggingMode)
	: Blueprint(NULL)
	, Graph(InGraph)
	, Node(InNode)
	, Pin(InPin)
	, MenuBuilder(InMenuBuilder)
	, bIsDebugging(bInDebuggingMode)
{
#if WITH_EDITOR
	Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
#endif

	if (Pin != NULL)
	{
		Node = Pin->GetOwningNode();
	}
}

/////////////////////////////////////////////////////
// UEdGraphNode

UEdGraphNode::UEdGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AdvancedPinDisplay(ENodeAdvancedPins::NoPins)
	, bIsNodeEnabled(true)
{

#if WITH_EDITORONLY_DATA
	bCommentBubblePinned = false;
	bCommentBubbleVisible = false;
	bCanResizeNode = false;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
UEdGraphPin* UEdGraphNode::CreatePin(EEdGraphPinDirection Dir, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, bool bIsArray, bool bIsReference, const FString& PinName, bool bIsConst /*= false*/, int32 Index /*= INDEX_NONE*/)
{
#if 0
	UEdGraphPin* NewPin = AllocatePinFromPool(this);
#else
	UEdGraphPin* NewPin = NewObject<UEdGraphPin>(this);
#endif
	NewPin->PinName = PinName;
	NewPin->Direction = Dir;
	NewPin->PinType.PinCategory = PinCategory;
	NewPin->PinType.PinSubCategory = PinSubCategory;
	NewPin->PinType.PinSubCategoryObject = PinSubCategoryObject;
	NewPin->PinType.bIsArray = bIsArray;
	NewPin->PinType.bIsReference = bIsReference;
	NewPin->PinType.bIsConst = bIsConst;
	NewPin->SetFlags(RF_Transactional);

	if (HasAnyFlags(RF_Transient))
	{
		NewPin->SetFlags(RF_Transient);
	}

	Modify(false);
	if ( Pins.IsValidIndex( Index ) )
	{
		Pins.Insert(NewPin, Index);
	}
	else
	{
		Pins.Add(NewPin);
	}
	return NewPin;
}

UEdGraphPin* UEdGraphNode::FindPin(const FString& PinName) const
{
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		if( Pins[PinIdx]->PinName == PinName )
		{
			return Pins[PinIdx];
		}
	}

	return NULL;
}

UEdGraphPin* UEdGraphNode::FindPinChecked(const FString& PinName) const
{
	UEdGraphPin* Result = FindPin(PinName);
	check(Result != NULL);
	return Result;
}

bool UEdGraphNode::RemovePin(UEdGraphPin* Pin)
{
	check( Pin );
	
	Modify();
	UEdGraphPin* RootPin = (Pin->ParentPin != nullptr)? Pin->ParentPin : Pin;
	RootPin->BreakAllPinLinks();

	if (Pins.Remove( RootPin ))
	{
		// Remove any children pins to ensure the entirety of the pin's representation is removed
		for (UEdGraphPin* ChildPin : RootPin->SubPins)
		{
			Pins.Remove(ChildPin);
			ChildPin->Modify();
			ChildPin->BreakAllPinLinks();
		}
		return true;
	}
	return false;
}

void UEdGraphNode::BreakAllNodeLinks()
{
	TSet<UEdGraphNode*> NodeList;

	NodeList.Add(this);

	// Iterate over each pin and break all links
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		Pins[PinIdx]->BreakAllPinLinks();
		NodeList.Add(Pins[PinIdx]->GetOwningNode());
	}

	// Send all nodes that received a new pin connection a notification
	for (auto It = NodeList.CreateConstIterator(); It; ++It)
	{
		UEdGraphNode* Node = (*It);
		Node->NodeConnectionListChanged();
	}
}

void UEdGraphNode::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	ensure(Pin.GetOwningNode() == this);
	HoverTextOut = Pin.PinToolTip;
}

void UEdGraphNode::SnapToGrid(float GridSnapSize)
{
	NodePosX = GridSnapSize * FMath::RoundToInt(NodePosX/GridSnapSize);
	NodePosY = GridSnapSize * FMath::RoundToInt(NodePosY/GridSnapSize);
}

class UEdGraph* UEdGraphNode::GetGraph() const
{
	UEdGraph* Graph = Cast<UEdGraph>(GetOuter());
	if (Graph == NULL)
	{
		ensureMsgf(false, TEXT("EdGraphNode::GetGraph : '%s' does not have a UEdGraph as an Outer."), *GetPathName());
	}
	return Graph;
}

void UEdGraphNode::DestroyNode()
{
	UEdGraph* ParentGraph = GetGraph();
	check(ParentGraph);

	// Remove the node - this will break all links. Will be GC'd after this.
	ParentGraph->RemoveNode(this);
}

const class UEdGraphSchema* UEdGraphNode::GetSchema() const
{
	UEdGraph* ParentGraph = GetGraph();
	return ParentGraph ? ParentGraph->GetSchema() : NULL;
}

bool UEdGraphNode::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	return CanCreateUnderSpecifiedSchema(Graph->GetSchema());
}

FLinearColor UEdGraphNode::GetNodeTitleColor() const
{
	return FLinearColor(0.4f, 0.62f, 1.0f);
}

FLinearColor UEdGraphNode::GetNodeCommentColor() const
{
	return FLinearColor::White;
}

FText UEdGraphNode::GetTooltipText() const
{
	return GetClass()->GetToolTipText();
}

FString UEdGraphNode::GetDocumentationExcerptName() const
{
	// Default the node to searching for an excerpt named for the C++ node class name, including the U prefix.
	// This is done so that the excerpt name in the doc file can be found by find-in-files when searching for the full class name.
	UClass* MyClass = GetClass();
	return FString::Printf(TEXT("%s%s"), MyClass->GetPrefixCPP(), *MyClass->GetName());
}


FString UEdGraphNode::GetDescriptiveCompiledName() const
{
	return GetFName().GetPlainNameString();
}

bool UEdGraphNode::IsDeprecated() const
{
	return GetClass()->HasAnyClassFlags(CLASS_Deprecated);
}

FString UEdGraphNode::GetDeprecationMessage() const
{
	return NSLOCTEXT("EdGraphCompiler", "NodeDeprecated_Warning", "@@ is deprecated; please replace or remove it.").ToString();
}

// Array of pooled pins
TArray<UEdGraphPin*> UEdGraphNode::PooledPins;

void UEdGraphNode::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UEdGraphNode* This = CastChecked<UEdGraphNode>(InThis);	

	// Only register the pool once per GC pass
	if (This->HasAnyFlags(RF_ClassDefaultObject))
	{
		if (This->GetClass() == UEdGraphNode::StaticClass())
		{
			for (int32 Index = 0; Index < PooledPins.Num(); ++Index)
			{
				Collector.AddReferencedObject(PooledPins[Index], This);
			}
		}
	}
	Super::AddReferencedObjects(This, Collector);
}

void UEdGraphNode::PostLoad()
{
	Super::PostLoad();

	// Create Guid if not present (and not CDO)
	if(!NodeGuid.IsValid() && !IsTemplate() && GetLinker() && GetLinker()->IsPersistent() && GetLinker()->IsLoading())
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Node '%s' missing NodeGuid."), *GetPathName());

		// Generate new one
		CreateNewGuid();
	}

	// Duplicating a Blueprint needs to have a new Node Guid generated, which was not occuring before this version
	if(GetLinkerUE4Version() < VER_UE4_POST_DUPLICATE_NODE_GUID)
	{
		// Generate new one
		CreateNewGuid();
	}
	// Moving to the new style comments requires conversion to preserve previous state
	if(GetLinkerUE4Version() < VER_UE4_GRAPH_INTERACTIVE_COMMENTBUBBLES)
	{
		bCommentBubbleVisible = !NodeComment.IsEmpty();
	}
}

void UEdGraphNode::CreateNewGuid()
{
	NodeGuid = FGuid::NewGuid();
}

void UEdGraphNode::FindDiffs( class UEdGraphNode* OtherNode, struct FDiffResults& Results ) 
{
}

UEdGraphPin* UEdGraphNode::AllocatePinFromPool(UEdGraphNode* OuterNode)
{
	if (PooledPins.Num() > 0)
	{
		UEdGraphPin* Result = PooledPins.Pop();
		Result->Rename(NULL, OuterNode);
		return Result;
	}
	else
	{
		UEdGraphPin* Result = NewObject<UEdGraphPin>(OuterNode);
		return Result;
	}
}

void UEdGraphNode::ReturnPinToPool(UEdGraphPin* OldPin)
{
	check(OldPin);

	check(!OldPin->HasAnyFlags(RF_NeedLoad));
	OldPin->ClearFlags(RF_NeedPostLoadSubobjects|RF_NeedPostLoad);

	OldPin->ResetToDefaults();

	PooledPins.Add(OldPin);
}

bool UEdGraphNode::CanDuplicateNode() const
{
	return true;
}

bool UEdGraphNode::CanUserDeleteNode() const
{
	return true;
}

FText UEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(GetClass()->GetName());
}

UObject* UEdGraphNode::GetJumpTargetForDoubleClick() const
{
	return NULL;
}

FText UEdGraphNode::GetPinDisplayName(const UEdGraphPin* Pin) const
{
	return GetSchema()->GetPinDisplayName(Pin);
}

int32 UEdGraphNode::GetPinIndex(UEdGraphPin* Pin) const
{
	return Pins.Find(Pin);
}

void UEdGraphNode::AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	// Searchable - Primary label for the item in the search results
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_Name, GetNodeTitle(ENodeTitleType::ListView)));

	// Searchable - As well as being searchable, this displays in the tooltip for the node
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_ClassName, FText::FromString(GetClass()->GetName())));

	// Non-searchable - Used to lookup the node when attempting to jump to it
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_NodeGuid, FText::FromString(NodeGuid.ToString(EGuidFormats::Digits))));

	// Non-searchable - Important for matching pin types with icons and colors, stored here so that each pin does not store it
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_SchemaName, FText::FromString(GetSchema()->GetClass()->GetName())));

	// Non-Searchable - Used to display the icon and color for this node for better visual identification.
	FLinearColor GlyphColor = FLinearColor::White;
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_Glyph, FText::FromString(GetPaletteIcon(GlyphColor).ToString())));
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_GlyphColor, FText::FromString(GlyphColor.ToString())));
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_Comment, FText::FromString(NodeComment)));
}

void UEdGraphNode::OnUpdateCommentText( const FString& NewComment )
{
	if( !NodeComment.Equals( NewComment ))
	{
		const FScopedTransaction Transaction( LOCTEXT( "CommentCommitted", "Comment Changed" ) );
		Modify();
		NodeComment	= NewComment;
	}
}

FText UEdGraphNode::GetKeywords() const
{
	return GetClass()->GetMetaDataText(TEXT("Keywords"), TEXT("UObjectKeywords"), GetClass()->GetFullGroupName(false));
}

#endif	//#if WITH_EDITOR

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
