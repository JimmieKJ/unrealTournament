// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EditorWidgetsPrivatePCH.h"
#include "SAssetDropTarget.h"
#include "AssetDragDropOp.h"
#include "ActorDragDropOp.h"
#include "AssetSelection.h"

void SAssetDropTarget::Construct(const FArguments& InArgs )
{
	OnAssetDropped = InArgs._OnAssetDropped;
	OnIsAssetAcceptableForDrop = InArgs._OnIsAssetAcceptableForDrop;

	bIsDragEventRecognized = false;
	bAllowDrop = false;

	SBorder::Construct( 
		SBorder::FArguments()
		.Padding(0)
		.BorderImage( this, &SAssetDropTarget::GetDragBorder )
		.BorderBackgroundColor( this, &SAssetDropTarget::GetDropBorderColor )
		[
			InArgs._Content.Widget
		]
	);
}

const FSlateBrush* SAssetDropTarget::GetDragBorder() const
{
	return bIsDragEventRecognized ? FEditorStyle::GetBrush("MaterialList.DragDropBorder") : FEditorStyle::GetBrush("NoBorder");
}

FSlateColor SAssetDropTarget::GetDropBorderColor() const
{
	return bIsDragEventRecognized ? 
		bAllowDrop ? FLinearColor(0,1,0,1) : FLinearColor(1,0,0,1)
		: FLinearColor::White;
}

FReply SAssetDropTarget::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bool bRecognizedEvent = false;
	UObject* Object = GetDroppedObject( DragDropEvent, bRecognizedEvent );

	// Not being dragged over by a recognizable event
	bIsDragEventRecognized = bRecognizedEvent;

	bAllowDrop = false;

	if( Object )
	{
		// Check and see if its valid to drop this object
		if( OnIsAssetAcceptableForDrop.IsBound() )
		{
			bAllowDrop = OnIsAssetAcceptableForDrop.Execute( Object );
		}
		else
		{
			// If no delegate is bound assume its always valid to drop this object
			bAllowDrop = true;
		}
	}
	else
	{
		// No object so we dont allow dropping
		bAllowDrop = false;
	}

	// Handle the reply if we are allowed to drop, otherwise do not handle it.
	return bAllowDrop ? FReply::Handled() : FReply::Unhandled();
}

FReply SAssetDropTarget::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// We've dropped an asset so we are no longer being dragged over
	bIsDragEventRecognized = false;

	// if we allow drop, call a delegate to handle the drop
	if( bAllowDrop )
	{
		bool bUnused;
		UObject* Object = GetDroppedObject( DragDropEvent, bUnused );

		if( Object )
		{
			OnAssetDropped.ExecuteIfBound( Object );
		}
	}

	return FReply::Handled();
}

void SAssetDropTarget::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// initially we dont recognize this event
	bIsDragEventRecognized = false;
}

void SAssetDropTarget::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	// No longer being dragged over
	bIsDragEventRecognized = false;
	// Disallow dropping if not dragged over.
	bAllowDrop = false;
}

UObject* SAssetDropTarget::GetDroppedObject( const FDragDropEvent& DragDropEvent, bool& bOutRecognizedEvent )
{
	bOutRecognizedEvent = false;
	UObject* DroppedObject = NULL;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	// Asset being dragged from content browser
	if (Operation->IsOfType<FAssetDragDropOp>())
	{
		bOutRecognizedEvent = true;
		TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

		bool bCanDrop = DragDropOp->AssetData.Num() == 1;

		if( bCanDrop )
		{
			const FAssetData& AssetData = DragDropOp->AssetData[0];

			// Make sure the asset is loaded
			DroppedObject = AssetData.GetAsset();
		}
	}
	// Asset being dragged from some external source
	else if (Operation->IsOfType<FExternalDragOperation>())
	{
		TArray<FAssetData> DroppedAssetData = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);

		if (DroppedAssetData.Num() == 1)
		{
			bOutRecognizedEvent = true;
			DroppedObject = DroppedAssetData[0].GetAsset();
		}
	}
	// Actor being dragged?
	else if (Operation->IsOfType<FActorDragDropOp>())
	{
		bOutRecognizedEvent = true;
		TSharedPtr<FActorDragDropOp> ActorDragDrop = StaticCastSharedPtr<FActorDragDropOp>(Operation);

		if (ActorDragDrop->Actors.Num() == 1)
		{
			DroppedObject = ActorDragDrop->Actors[0].Get();
		}
	}

	return DroppedObject;
}
