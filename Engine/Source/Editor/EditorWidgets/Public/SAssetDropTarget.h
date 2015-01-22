// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * A widget that displays a hover cue and handles dropping assets of allowed types onto this widget
 */
class EDITORWIDGETS_API SAssetDropTarget : public SBorder
{
public:
	/** Called when a valid asset is dropped */
	DECLARE_DELEGATE_OneParam( FOnAssetDropped, UObject* );

	/** Called when we need to check if an asset type is valid for dropping */
	DECLARE_DELEGATE_RetVal_OneParam( bool, FIsAssetAcceptableForDrop, const UObject* );

	SLATE_BEGIN_ARGS( SAssetDropTarget )
	{}
		/* Content to display for the in the drop target */
		SLATE_DEFAULT_SLOT( FArguments, Content )
		/** Called when a valid asset is dropped */
		SLATE_EVENT( FOnAssetDropped, OnAssetDropped )
		/** Called to check if an asset is acceptible for dropping */
		SLATE_EVENT( FIsAssetAcceptableForDrop, OnIsAssetAcceptableForDrop )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs );
private:
	const FSlateBrush* GetDragBorder() const;
	FSlateColor GetDropBorderColor() const;

	// SWidget interface
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	// End of SWidget interface

	/**
	 * Gets the dropped object from a drag drop event
	 */
	UObject* GetDroppedObject( const FDragDropEvent& DragDropEvent, bool& bOutRecognizedEvent );
private:
	/** Delegate to call when an asset is dropped */
	FOnAssetDropped OnAssetDropped;
	/** Delegate to call to check validity of the asset */
	FIsAssetAcceptableForDrop OnIsAssetAcceptableForDrop;
	/** Whether or not we are being dragged over by a recognized event*/
	bool bIsDragEventRecognized;
	/** Whether or not we currently allow dropping */
	bool bAllowDrop;
};