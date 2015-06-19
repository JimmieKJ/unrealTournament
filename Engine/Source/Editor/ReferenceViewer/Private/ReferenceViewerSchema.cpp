// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "AssetThumbnail.h"
#include "ReferenceViewerActions.h"
#include "GlobalEditorCommonCommands.h"

#include "Editor/GraphEditor/Public/ConnectionDrawingPolicy.h"

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

	MenuBuilder->AddSubMenu(NSLOCTEXT("ReferenceViewerSchema","MakeCollectionWithReferencedAssetsTitle", "Make Collection with Referenced Assets"),
							FText::GetEmpty(),
							FNewMenuDelegate::CreateUObject(this, &UReferenceViewerSchema::GetMakeCollectionWithReferencedAssetsSubMenu )
							);

	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().ShowSizeMap);
	MenuBuilder->AddMenuEntry(FReferenceViewerActions::Get().ShowReferenceTree);
}

FLinearColor UReferenceViewerSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
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

void UReferenceViewerSchema::GetMakeCollectionWithReferencedAssetsSubMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakeLocalCollectionWithReferencedAssets);
	MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakePrivateCollectionWithReferencedAssets);
	MenuBuilder.AddMenuEntry(FReferenceViewerActions::Get().MakeSharedCollectionWithReferencedAssets);
}
