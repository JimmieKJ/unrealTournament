// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "AssetThumbnail.h"
#include "ReferenceViewerActions.h"
#include "GlobalEditorCommonCommands.h"

#include "Editor/GraphEditor/Public/ConnectionDrawingPolicy.h"


static const FLinearColor RiceFlower = FLinearColor(FColor(236, 252, 227));
static const FLinearColor CannonPink = FLinearColor(FColor(145, 66, 117));

// Overridden connection drawing policy to use less curvy lines between nodes
class FReferenceViewerConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FReferenceViewerConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	{
	}

	virtual FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const override
	{
		const int32 Tension = FMath::Abs<int32>(Start.X - End.X);
		return Tension * FVector2D(1.0f, 0);
	}

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override
	{
		if (OutputPin->PinType.PinCategory == TEXT("hard") || InputPin->PinType.PinCategory == TEXT("hard"))
		{
			Params.WireColor = RiceFlower;
		}
		else
		{
			Params.WireColor = CannonPink;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// UReferenceViewerSchema

UReferenceViewerSchema::UReferenceViewerSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UReferenceViewerSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	MenuBuilder->AddMenuEntry(FGlobalEditorCommonCommands::Get().FindInContentBrowser);
	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().OpenSelectedInAssetEditor);
	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().ReCenterGraph);
	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().ListReferencedObjects);
	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().ListObjectsThatReference);

	MenuBuilder->AddSubMenu(NSLOCTEXT("ReferenceViewerSchema","MakeCollectionWithTitle", "Make Collection with"),
							NSLOCTEXT("ReferenceViewerSchema","MakeCollectionWithTooltip", "Makes a collection with either the referencers or dependencies of the selected nodes."),
							FNewMenuDelegate::CreateUObject(this, &UReferenceViewerSchema::GetMakeCollectionWithSubMenu )
							);

	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().ShowSizeMap);
	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().ShowReferenceTree);
}

FLinearColor UReferenceViewerSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	if (PinType.PinCategory == TEXT("hard"))
	{
		return RiceFlower;
	}
	else
	{
		return CannonPink;
	}
}

void UReferenceViewerSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	// Don't allow breaking any links
}

void UReferenceViewerSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	// Don't allow breaking any links
}

FPinConnectionResponse UReferenceViewerSchema::MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsItermeadiateMove) const
{
	// Don't allow moving any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FPinConnectionResponse UReferenceViewerSchema::CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsItermeadiateCopy) const
{
	// Don't allow copying any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FConnectionDrawingPolicy* UReferenceViewerSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FReferenceViewerConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements);
}

void UReferenceViewerSchema::GetMakeCollectionWithSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddSubMenu(NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithReferencersTitle", "Referencers <-"),
		NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithReferencersTooltip", "Makes a collection with assets one connection to the left of selected nodes."),
		FNewMenuDelegate::CreateUObject(this, &UReferenceViewerSchema::GetMakeCollectionWithReferencersOrDependenciesSubMenu, true)
		);

	MenuBuilder.AddSubMenu(NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithDependenciesTitle", "Dependencies ->"),
		NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithDependenciesTooltip", "Makes a collection with assets one connection to the right of selected nodes."),
		FNewMenuDelegate::CreateUObject(this, &UReferenceViewerSchema::GetMakeCollectionWithReferencersOrDependenciesSubMenu, false)
		);
}

void UReferenceViewerSchema::GetMakeCollectionWithReferencersOrDependenciesSubMenu(FMenuBuilder& MenuBuilder, bool bReferencers)
{
	if (bReferencers)
	{
		MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakeLocalCollectionWithReferencers, 
			NAME_None, TAttribute<FText>(), 
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Local), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Local))
			);
		MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakePrivateCollectionWithReferencers,
			NAME_None, TAttribute<FText>(), 
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Private), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Private))
			);
		MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakeSharedCollectionWithReferencers,
			NAME_None, TAttribute<FText>(), 
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Shared), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Shared))
			);
	}
	else
	{
		MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakeLocalCollectionWithDependencies, 
			NAME_None, TAttribute<FText>(), 
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Local), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Local))
			);
		MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakePrivateCollectionWithDependencies,
			NAME_None, TAttribute<FText>(), 
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Private), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Private))
			);
		MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakeSharedCollectionWithDependencies,
			NAME_None, TAttribute<FText>(), 
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Shared), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Shared))
			);
	}
}
