// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorActions.h"
#include "EnvironmentQueryConnectionDrawingPolicy.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_Composite.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GenericCommands.h"

#include "AssetRegistryModule.h"
#include "HotReloadInterface.h"

#define LOCTEXT_NAMESPACE "EnvironmentQuerySchema"
#define SNAP_GRID (16) // @todo ensure this is the same as SNodePanel::GetSnapGridSize()

namespace 
{
	// Maximum distance a drag can be off a node edge to require 'push off' from node
	const int32 NodeDistance = 60;
}

UEdGraphNode* FEnvironmentQuerySchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
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

			if (XDelta < NodeDistance)
			{
				// Set location to edge of current node minus the max move distance
				// to force node to push off from connect node enough to give selection handle
				XLocation = PinNode->NodePosX - NodeDistance;
			}
		}

		NodeTemplate->NodePosX = XLocation;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(SNAP_GRID);

		ResultNode = NodeTemplate;
	}

	return ResultNode;
}

UEdGraphNode* FEnvironmentQuerySchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode) 
{
	// I have no idea if ResultNode is meaningful to assign here and return it.
	// But every other override of this function is not void, so I'm making this one not void
	// because the compiler complains otherwise.
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

void FEnvironmentQuerySchemaAction_NewNode::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( NodeTemplate );
}


UEdGraphNode* FEnvironmentQuerySchemaAction_NewSubNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	ParentNode->AddSubNode(NodeTemplate,ParentGraph);
	return NULL;
}

UEdGraphNode* FEnvironmentQuerySchemaAction_NewSubNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode) 
{
	return PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
}

void FEnvironmentQuerySchemaAction_NewSubNode::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( NodeTemplate );
}

//////////////////////////////////////////////////////////////////////////

namespace FEQSClassCacheInfo
{
	static bool bEQSGeneratorCacheInvalid = true;
	static bool bEQSTestCacheInvalid = true;
	void InvalidateCache()
	{
		bEQSGeneratorCacheInvalid = true;
		bEQSTestCacheInvalid = true;
	}

	void InvalidateCache(const class FAssetData& AssetData)
	{
		bEQSGeneratorCacheInvalid = true;
		bEQSTestCacheInvalid = true;
	}

	void InvalidateCache(bool bWasTriggeredAutomatically)
	{
		bEQSGeneratorCacheInvalid = true;
		bEQSTestCacheInvalid = true;
	}

	void SetupCacheObservers()
	{
		static bool bCacheFixDelegatesSetUp = false;
		if (bCacheFixDelegatesSetUp == false)
		{
			bCacheFixDelegatesSetUp = true;

			// Register with the Asset Registry to be informed when it is done loading up files.
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			AssetRegistryModule.Get().OnFilesLoaded().AddStatic(&FEQSClassCacheInfo::InvalidateCache);
			AssetRegistryModule.Get().OnAssetAdded().AddStatic(&FEQSClassCacheInfo::InvalidateCache);
			AssetRegistryModule.Get().OnAssetRemoved().AddStatic(&FEQSClassCacheInfo::InvalidateCache);

			// Register to have Populate called when doing a Hot Reload.
			IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
			HotReloadSupport.OnHotReload().AddStatic(&FEQSClassCacheInfo::InvalidateCache);

			// Register to have Populate called when a Blueprint is compiled.
			GEditor->OnBlueprintCompiled().AddStatic(&FEQSClassCacheInfo::InvalidateCache);
			GEditor->OnClassPackageLoadedOrUnloaded().AddStatic(&FEQSClassCacheInfo::InvalidateCache);
		}
	}
}

UEdGraphSchema_EnvironmentQuery::UEdGraphSchema_EnvironmentQuery(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<FEnvironmentQuerySchemaAction_NewNode> AddNewNodeAction(FGraphContextMenuBuilder& ContextMenuBuilder, const FString& Category, const FText& MenuDesc, const FString& Tooltip)
{
	const TSharedRef<FEnvironmentQuerySchemaAction_NewNode> NewAction = MakeShareable( new FEnvironmentQuerySchemaAction_NewNode(Category, MenuDesc, Tooltip, 0) );
	ContextMenuBuilder.AddAction( NewAction );
	return NewAction;
}

TSharedPtr<FEnvironmentQuerySchemaAction_NewSubNode> AddNewSubNodeAction(FGraphContextMenuBuilder& ContextMenuBuilder, const FString& Category, const FText& MenuDesc, const FString& Tooltip)
{
	TSharedPtr<FEnvironmentQuerySchemaAction_NewSubNode> NewAction = MakeShareable(new FEnvironmentQuerySchemaAction_NewSubNode(Category, MenuDesc, Tooltip, 0));
	ContextMenuBuilder.AddAction( NewAction );
	return NewAction;
}

void UEdGraphSchema_EnvironmentQuery::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	FGraphNodeCreator<UEnvironmentQueryGraphNode_Root> NodeCreator(Graph);
	UEnvironmentQueryGraphNode_Root* MyNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	SetNodeMetaData(MyNode, FNodeMetadata::DefaultGraphNode);
}

FString GetNodeDescriptionHelper(UObject* Ob)
{
	if (Ob == NULL)
	{
		return TEXT("unknown");
	}

	const UClass* ObClass = Ob->IsA(UClass::StaticClass()) ? (const UClass*)Ob : Ob->GetClass();
	if (ObClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return ObClass->GetName().LeftChop(2);
	}

	FString TypeDesc = ObClass->GetName();
	const int32 ShortNameIdx = TypeDesc.Find(TEXT("_"));
	if (ShortNameIdx != INDEX_NONE)
	{
		TypeDesc = TypeDesc.Mid(ShortNameIdx + 1);
	}

	return TypeDesc;
}

static void GatherEQSGenerators(TArray<UClass*>& Classes)
{
	static TArray<UClass*> CachedClasses;
	if (CachedClasses.Num() == 0 || FEQSClassCacheInfo::bEQSGeneratorCacheInvalid)
	{
		FEQSClassCacheInfo::SetupCacheObservers();
		FEQSClassCacheInfo::bEQSGeneratorCacheInvalid = false;
		CachedClasses.Reset();

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* TestClass = *It;
			if (TestClass->HasAnyClassFlags(CLASS_Abstract) ||
				// ignore temporary BP classes
				(!TestClass->HasAnyClassFlags(CLASS_Native) && (FKismetEditorUtilities::IsClassABlueprintSkeleton(TestClass) || TestClass->HasAnyClassFlags(CLASS_NewerVersionExists))) || 
				!TestClass->IsChildOf(UEnvQueryGenerator::StaticClass()) ||
				TestClass == UEnvQueryGenerator_Composite::StaticClass())
			{
				continue;
			}

			CachedClasses.Add(TestClass);
		}
	}

	Classes.Append(CachedClasses);
}

static void GatherEQSTests(TArray<UClass*>& Classes)
{
	static TArray<UClass*> CachedClasses;
	if (CachedClasses.Num() == 0 || FEQSClassCacheInfo::bEQSTestCacheInvalid)
	{
		FEQSClassCacheInfo::SetupCacheObservers();
		FEQSClassCacheInfo::bEQSTestCacheInvalid = false;
		CachedClasses.Reset();

		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* TestClass = *It;
			if (TestClass->HasAnyClassFlags(CLASS_Abstract) ||
				!TestClass->HasAnyClassFlags(CLASS_Native) ||
				!TestClass->IsChildOf(UEnvQueryTest::StaticClass()))
			{
				continue;
			}

			CachedClasses.Add(TestClass);
		}
	}

	Classes.Append(CachedClasses);
}

void UEdGraphSchema_EnvironmentQuery::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	UEnvironmentQueryGraphNode* ParentGraphNode = ContextMenuBuilder.FromPin ? Cast<UEnvironmentQueryGraphNode>(ContextMenuBuilder.FromPin->GetOuter()) : NULL;
	if  (ParentGraphNode && !ParentGraphNode->IsA(UEnvironmentQueryGraphNode_Root::StaticClass()))
	{
		return;
	}

	TArray<UClass*> AllowedClasses;
	GatherEQSGenerators(AllowedClasses);
	
	for (int32 i = 0; i < AllowedClasses.Num(); i++)
	{
		TSharedPtr<FEnvironmentQuerySchemaAction_NewNode> AddOpAction = AddNewNodeAction(ContextMenuBuilder, "Generators", FText::FromString(GetNodeDescriptionHelper(AllowedClasses[i])), FString());
		
		UEnvironmentQueryGraphNode_Option* OpNode = NewObject<UEnvironmentQueryGraphNode_Option>(ContextMenuBuilder.OwnerOfTemporaries);
		OpNode->EnvQueryNodeClass = AllowedClasses[i];
		AddOpAction->NodeTemplate = OpNode;		
	}
}

void UEdGraphSchema_EnvironmentQuery::GetGraphNodeContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	UEnvironmentQueryGraphNode_Option* SelectedOption = Cast<UEnvironmentQueryGraphNode_Option>(ContextMenuBuilder.SelectedObjects[0]);
	UEdGraph* Graph = (UEdGraph*)ContextMenuBuilder.CurrentGraph;

	TArray<UClass*> AllowedClasses;
	GatherEQSTests(AllowedClasses);

	for (int32 i = 0; i < AllowedClasses.Num(); i++)
	{
		UEnvironmentQueryGraphNode_Test* OpNode = NewObject<UEnvironmentQueryGraphNode_Test>(Graph);
		OpNode->EnvQueryNodeClass = AllowedClasses[i];

		TSharedPtr<FEnvironmentQuerySchemaAction_NewSubNode> AddOpAction = AddNewSubNodeAction(ContextMenuBuilder, TEXT(""), FText::FromString(GetNodeDescriptionHelper(AllowedClasses[i])), "");
		AddOpAction->ParentNode = SelectedOption;
		AddOpAction->NodeTemplate = OpNode;
	}
}

void UEdGraphSchema_EnvironmentQuery::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	if (InGraphPin)
	{
		MenuBuilder->BeginSection("EnvironmentQueryGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
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
						FNewMenuDelegate::CreateUObject( (UEdGraphSchema_EnvironmentQuery*const)this, &UEdGraphSchema_EnvironmentQuery::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
				}
				else
				{
					((UEdGraphSchema_EnvironmentQuery*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
				}
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode)
	{
		MenuBuilder->BeginSection("EnvironmentQueryGraphSchemaNodeActions", LOCTEXT("ClassActionsMenuHeader", "Actions"));
		{
			MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
			MenuBuilder->AddMenuEntry(FGenericCommands::Get().Delete);
		}
		MenuBuilder->EndSection();
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}

void UEdGraphSchema_EnvironmentQuery::GetBreakLinkToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
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


const FPinConnectionResponse UEdGraphSchema_EnvironmentQuery::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are on the same node"));
	}

	if ((PinA->Direction == EGPD_Input && PinA->LinkedTo.Num()>0) || 
		(PinB->Direction == EGPD_Input && PinB->LinkedTo.Num()>0))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
	}

	// Compare the directions
	bool bDirectionsOK = false;

	if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Output))
	{
		bDirectionsOK = true;
	}
	else if ((PinB->Direction == EGPD_Input) && (PinA->Direction == EGPD_Output))
	{
		bDirectionsOK = true;
	}

	if (bDirectionsOK)
	{
		if ( (PinA->Direction == EGPD_Input && PinA->LinkedTo.Num()>0) || (PinB->Direction == EGPD_Input && PinB->LinkedTo.Num()>0))
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Already connected with other"));
		}
	}
	else
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
}

const FPinConnectionResponse UEdGraphSchema_EnvironmentQuery::CanMergeNodes(const UEdGraphNode* NodeA, const UEdGraphNode* NodeB) const
{
	// Make sure the nodes are not the same 
	if (NodeA == NodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are the same node"));
	}

	const bool bNodeAIsTest = NodeA->IsA(UEnvironmentQueryGraphNode_Test::StaticClass());
	const bool bNodeAIsOption = NodeA->IsA(UEnvironmentQueryGraphNode_Option::StaticClass());
	const bool bNodeBIsTest = NodeB->IsA(UEnvironmentQueryGraphNode_Test::StaticClass());
	const bool bNodeBIsOption = NodeB->IsA(UEnvironmentQueryGraphNode_Option::StaticClass());

	if (bNodeAIsTest && (bNodeBIsOption || bNodeBIsTest))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FLinearColor UEdGraphSchema_EnvironmentQuery::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FColor::White;
}

bool UEdGraphSchema_EnvironmentQuery::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	check(Pin != NULL);

	if (Pin->bDefaultValueIsIgnored)
	{
		return true;
	}

	return false;
}

class FConnectionDrawingPolicy* UEdGraphSchema_EnvironmentQuery::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FEnvironmentQueryConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

#undef LOCTEXT_NAMESPACE
