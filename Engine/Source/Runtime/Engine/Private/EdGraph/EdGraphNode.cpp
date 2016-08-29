// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EdGraph/EdGraph.h"
#include "BlueprintsObjectVersion.h"
#include "BlueprintUtilities.h"
#if WITH_EDITOR
#include "Editor/UnrealEd/Public/CookerSettings.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "SlateBasics.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/Kismet2/Kismet2NameValidators.h"
#include "Editor/Kismet/Public/FindInBlueprintManager.h"
#include "EditorStyle.h"
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
	, EnabledState(ENodeEnabledState::Enabled)
	, bUserSetEnabledState(false)
	, bIsNodeEnabled_DEPRECATED(true)
{

#if WITH_EDITORONLY_DATA
	bCommentBubblePinned = false;
	bCommentBubbleVisible = false;
	bCanResizeNode = false;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR

UEdGraphPin* UEdGraphNode::CreatePin(EEdGraphPinDirection Dir, const FEdGraphPinType& InPinType, const FString& PinName, int32 Index /*= INDEX_NONE*/)
{
	UEdGraphPin* NewPin = UEdGraphPin::CreatePin(this);
	NewPin->PinName = PinName;
	NewPin->Direction = Dir;

	NewPin->PinType = InPinType;

	Modify(false);
	if (Pins.IsValidIndex(Index))
	{
		Pins.Insert(NewPin, Index);
	}
	else
	{
		Pins.Add(NewPin);
	}
	return NewPin;
}

UEdGraphPin* UEdGraphNode::CreatePin(EEdGraphPinDirection Dir, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, bool bIsArray, bool bIsReference, const FString& PinName, bool bIsConst /*= false*/, int32 Index /*= INDEX_NONE*/)
{
	FEdGraphPinType PinType(PinCategory, PinSubCategory, PinSubCategoryObject, bIsArray, bIsReference);
	PinType.bIsConst = bIsConst;

	return CreatePin(Dir, PinType, PinName, Index);
}

UEdGraphPin* UEdGraphNode::FindPin(const FString& PinName, const EEdGraphPinDirection Direction) const
{
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		if( Pins[PinIdx]->PinName == PinName && (Direction == EGPD_MAX || Direction == Pins[PinIdx]->Direction))
		{
			return Pins[PinIdx];
		}
	}

	return NULL;
}

UEdGraphPin* UEdGraphNode::FindPinChecked(const FString& PinName, const EEdGraphPinDirection Direction) const
{
	UEdGraphPin* Result = FindPin(PinName, Direction);
	check(Result != NULL);
	return Result;
}

UEdGraphPin* UEdGraphNode::FindPinById(const FGuid PinId) const
{
	for (int32 PinIdx = 0; PinIdx < Pins.Num(); PinIdx++)
	{
		if (Pins[PinIdx]->PinId == PinId)
		{
			return Pins[PinIdx];
		}
	}

	return NULL;
}

UEdGraphPin* UEdGraphNode::FindPinByIdChecked(const FGuid PinId) const
{
	UEdGraphPin* Result = FindPinById(PinId);
	check(Result != NULL);
	return Result;
}

bool UEdGraphNode::RemovePin(UEdGraphPin* Pin)
{
	check( Pin );
	
	Modify();
	UEdGraphPin* RootPin = (Pin->ParentPin != nullptr) ? Pin->ParentPin : Pin;
	RootPin->MarkPendingKill();

	if (Pins.Remove( RootPin ))
	{
		// Remove any children pins to ensure the entirety of the pin's representation is removed
		for (UEdGraphPin* ChildPin : RootPin->SubPins)
		{
			Pins.Remove(ChildPin);
			ChildPin->MarkPendingKill();
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
	if (Graph == NULL && !IsPendingKill())
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

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FSlateIcon UEdGraphNode::GetIconAndTint(FLinearColor& OutColor) const
{
	// @todo: Remove with GetPaletteIcon
	FName DeprecatedName = GetPaletteIcon(OutColor);
	if (!DeprecatedName.IsNone())
	{
		return FSlateIcon("EditorStyle", DeprecatedName);
	}
	
	static const FSlateIcon Icon = FSlateIcon("EditorStyle", "GraphEditor.Default_16x");
	return Icon;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

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

void UEdGraphNode::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UEdGraphNode* This = CastChecked<UEdGraphNode>(InThis);
	for (UEdGraphPin* Pin : This->Pins)
	{
		if (Pin)
		{
			Pin->AddStructReferencedObjects(Collector);
		}
	}
}

void UEdGraphNode::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FBlueprintsObjectVersion::GUID);

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		// If this was an older version, ensure that we update the enabled state for already-disabled nodes.
		// Note: We need to do this here and not in PostLoad() as it must be assigned prior to compile-on-load.
		if(!bIsNodeEnabled_DEPRECATED && !bUserSetEnabledState && EnabledState == ENodeEnabledState::Enabled)
		{
			EnabledState = ENodeEnabledState::Disabled;
		}

		if (Ar.IsPersistent() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
		{
			if (Ar.CustomVer(FBlueprintsObjectVersion::GUID) < FBlueprintsObjectVersion::EdGraphPinOptimized)
			{
				for (UEdGraphPin_Deprecated* LegacyPin : DeprecatedPins)
				{
					Ar.Preload(LegacyPin);
					if (UEdGraphPin::FindPinCreatedFromDeprecatedPin(LegacyPin) == nullptr)
					{
						UEdGraphPin::CreatePinFromDeprecatedPin(LegacyPin);
					}
				}
			}
		}
	}

	if (Ar.CustomVer(FBlueprintsObjectVersion::GUID) >= FBlueprintsObjectVersion::EdGraphPinOptimized)
	{
		UEdGraphPin::SerializeAsOwningNode(Ar, Pins);
	}
}

void UEdGraphNode::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

#if WITH_EDITORONLY_DATA
	if (!NodeUpgradeMessage.IsEmpty())
	{
		// When saving, we clear any upgrade messages
		NodeUpgradeMessage = FText::GetEmpty();
	}
#endif // WITH_EDITORONLY_DATA
}

void UEdGraphNode::PostLoad()
{
	Super::PostLoad();

	// Create Guid if not present (and not CDO)
	if(!NodeGuid.IsValid() && !IsTemplate() && GetLinker() && GetLinker()->IsPersistent() && GetLinker()->IsLoading())
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Node '%s' missing NodeGuid, this can cause deterministic cooking issues please resave package."), *GetPathName());

		// Generate new one
		CreateNewGuid();
	}

	// Duplicating a Blueprint needs to have a new Node Guid generated, which was not occuring before this version
	if(GetLinkerUE4Version() < VER_UE4_POST_DUPLICATE_NODE_GUID)
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Node '%s' missing NodeGuid because of upgrade from old package version, this can cause deterministic cooking issues please resave package."), *GetPathName());

		// Generate new one
		CreateNewGuid();
	}
	// Moving to the new style comments requires conversion to preserve previous state
	if(GetLinkerUE4Version() < VER_UE4_GRAPH_INTERACTIVE_COMMENTBUBBLES)
	{
		bCommentBubbleVisible = !NodeComment.IsEmpty();
	}

	if (DeprecatedPins.Num())
	{
		for (UEdGraphPin_Deprecated* LegacyPin : DeprecatedPins)
		{
			LegacyPin->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders);
			LegacyPin->SetFlags(RF_Transient);
			LegacyPin->MarkPendingKill();
		}

		DeprecatedPins.Empty();
	}
}

void UEdGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(const UEdGraphSchema* Schema = GetSchema())
	{
		Schema->ForceVisualizationCacheClear();
	}
}

void UEdGraphNode::PostEditUndo()
{
	UEdGraphPin::ResolveAllPinReferences();
	
	return UObject::PostEditUndo();
}

void UEdGraphNode::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	Super::ExportCustomProperties(Out, Indent);

	for (const UEdGraphPin* Pin : Pins)
	{
		FString PinString;
		Pin->ExportTextItem(PinString, PPF_Delimited);
		Out.Logf(TEXT("%sCustomProperties Pin %s\r\n"), FCString::Spc(Indent), *PinString);
	}
}

void UEdGraphNode::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	Super::ImportCustomProperties(SourceText, Warn);

	if (FParse::Command(&SourceText, TEXT("Pin")))
	{
		UEdGraphPin* NewPin = UEdGraphPin::CreatePin(this);
		const bool bParseSuccess = NewPin->ImportTextItem(SourceText, PPF_Delimited, nullptr, GWarn);
		if (bParseSuccess)
		{
			Pins.Add(NewPin);
		}
		else
		{
			// Still adding a nullptr to preserve indices
			Pins.Add(nullptr);
		}
	}
}

void UEdGraphNode::BeginDestroy()
{
	for (UEdGraphPin* Pin : Pins)
	{
		Pin->MarkPendingKill();
	}

	Pins.Empty();

	Super::BeginDestroy();
}

void UEdGraphNode::CreateNewGuid()
{
	NodeGuid = FGuid::NewGuid();
}

void UEdGraphNode::FindDiffs( class UEdGraphNode* OtherNode, struct FDiffResults& Results ) 
{
}

void UEdGraphNode::DestroyPin(UEdGraphPin* Pin)
{
	Pin->MarkPendingKill();
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

FString UEdGraphNode::GetFindReferenceSearchString() const
{
	return GetNodeTitle(ENodeTitleType::ListView).ToString();
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
	FSlateIcon Icon = GetIconAndTint(GlyphColor);
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_Glyph, FText::FromName(Icon.GetStyleName())));
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_GlyphStyleSet, FText::FromName(Icon.GetStyleSetName())));
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

void UEdGraphNode::AddNodeUpgradeNote(FText InUpgradeNote)
{
#if WITH_EDITORONLY_DATA
	if (NodeUpgradeMessage.IsEmpty())
	{
		NodeUpgradeMessage = InUpgradeNote;
	}
	else
	{
		NodeUpgradeMessage = FText::Format(FText::FromString(TEXT("{0}\n{1}")), NodeUpgradeMessage, InUpgradeNote);
	}
#endif
}
#endif	//#if WITH_EDITOR

bool UEdGraphNode::IsInDevelopmentMode() const
{
#if WITH_EDITOR
	// By default, development mode is implied when running in the editor and not cooking via commandlet, unless enabled in the project settings.
	return !IsRunningCommandlet() || GetDefault<UCookerSettings>()->bCompileBlueprintsInDevelopmentMode;
#else
	return false;
#endif
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
