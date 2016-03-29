// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

namespace HierarchyColumns
{
	/** IDs for list columns */
	static const FName ColumnID_LevelLabel( "Level" );
	static const FName ColumnID_Visibility( "Visibility" );
	static const FName ColumnID_Lock( "Lock" );
	static const FName ColumnID_SCCStatus( "SCC_Status" );
	static const FName ColumnID_Save( "Save" );
	static const FName ColumnID_Color("Color");
	static const FName ColumnID_Kismet( "Blueprint" );
	static const FName ColumnID_ActorCount( "ActorCount" );
	static const FName ColumnID_LightmassSize( "LightmassSize" );
	static const FName ColumnID_FileSize( "FileSize" );
}

/** A single item in the levels hierarchy tree. Represents a level model */
class SWorldHierarchyItem 
	: public SMultiColumnTableRow<TSharedPtr<FLevelModel>>
{
public:
	DECLARE_DELEGATE_TwoParams( FOnNameChanged, const TSharedPtr<FLevelModel>& /*TreeItem*/, const FVector2D& /*MessageLocation*/);

	SLATE_BEGIN_ARGS( SWorldHierarchyItem )
		: _IsItemExpanded( false )
	{}
		/** Data for the world */
		SLATE_ARGUMENT(TSharedPtr<FLevelCollectionModel>, InWorldModel)
	
		/** Item model this widget represents */
		SLATE_ARGUMENT(TSharedPtr<FLevelModel>, InItemModel)

		/** True when this item has children and is expanded */
		SLATE_ATTRIBUTE(bool, IsItemExpanded)

		/** The string in the title to highlight */
		SLATE_ATTRIBUTE(FText, HighlightText)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView);

	TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

	FReply OnItemDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;

private:
	/** Operations buttons enabled/disabled state */
	bool IsSaveEnabled() const;
	bool IsLockEnabled() const;
	bool IsVisibilityEnabled() const;
	bool IsKismetEnabled() const;

	/** Get DrawColor for the level */
	FSlateColor GetDrawColor() const;
		
	/**
	 *	Called when the user clicks on the visibility icon for a Level's item widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnToggleVisibility();

	/**
	 *	Called when the user clicks on the lock icon for a Level's item widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnToggleLock();

	/**
	 *	Called when the user clicks on the save icon for a Level's item widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnSave();

	/**
	 *	Called when the user clicks on the kismet icon for a Level's item widget
	 *
	 *	@return	A reply that indicated whether this event was handled.
	 */
	FReply OnOpenKismet();

	/**
	*	Called when the user clicks on the color icon for a Level's item widget
	*
	*	@return	A reply that indicated whether this event was handled.
	*/
	FReply OnChangeColor();

	/** Callback invoked from the color picker */
	void OnSetColorFromColorPicker(FLinearColor NewColor);
	void OnColorPickerCancelled(FLinearColor OriginalColor);
	void OnColorPickerInteractiveBegin();
	void OnColorPickerInteractiveEnd();

	/** Whether color button should be visible, depends on whether sub-level is loaded */
	EVisibility GetColorButtonVisibility() const;
	
	/**
	 *  @return The text of level name while it is not being edited
	 */
	FText GetLevelDisplayNameText() const;

	/**
	*  @return The tooltip of level name (shows the full path of the asset package)
	*/
	FText GetLevelDisplayNameTooltip() const;

	/** */
	FSlateFontInfo GetLevelDisplayNameFont() const;
	
	/** */
	FSlateColor GetLevelDisplayNameColorAndOpacity() const;

	/**
	 *	@return	The SlateBrush representing level icon
	 */
	const FSlateBrush* GetLevelIconBrush() const;

	/**
	 *	Called to get the Slate Image Brush representing the visibility state of
	 *	the Level this item widget represents
	 *
	 *	@return	The SlateBrush representing the Level's visibility state
	 */
	const FSlateBrush* GetLevelVisibilityBrush() const;

	/**
	 *	Called to get the Slate Image Brush representing the lock state of
	 *	the Level this item widget represents
	 *
	 *	@return	The SlateBrush representing the Level's lock state
	 */
	const FSlateBrush* GetLevelLockBrush() const;

	/**
	 *	Called to get the Slate Image Brush representing the save state of
	 *	the Level this row widget represents
	 *
	 *	@return	The SlateBrush representing the Level's save state
	 */
	const FSlateBrush* GetLevelSaveBrush() const;

	/**
	 *	Called to get the Slate Image Brush representing the kismet state of
	 *	the Level this row widget represents
	 *
	 *	@return	The SlateBrush representing the Level's kismet state
	 */
	const FSlateBrush* GetLevelKismetBrush() const;

	/**
	*	Called to get the Slate Image Brush representing the color of
	*	the Level this row widget represents
	*
	*	@return	The SlateBrush representing the Level's color
	*/
	const FSlateBrush* GetLevelColorBrush() const;

	
	/** */
	FText GetLevelLockToolTip() const;
	
	/** */
	FText GetSCCStateTooltip() const;
	
	/** */
	const FSlateBrush* GetSCCStateImage() const;

private:
	/** The world data */
	TSharedPtr<FLevelCollectionModel>	WorldModel;

	/** The data for this item */
	TSharedPtr<FLevelModel>			LevelModel;
	
	/** The string to highlight in level display name */
	TAttribute<FText>				HighlightText;

	/** True when this item has children and is expanded */
	TAttribute<bool>				IsItemExpanded;

	/** Brushes for the different streaming level types */
	const FSlateBrush*				StreamingLevelAlwaysLoadedBrush;
	const FSlateBrush*				StreamingLevelKismetBrush;

	/**	The visibility button for the Level */
	TSharedPtr<SButton>				VisibilityButton;

	/**	The lock button for the Level */
	TSharedPtr<SButton>				LockButton;

	/**	The save button for the Level */
	TSharedPtr<SButton>				SaveButton;

	/**	The kismet button for the Level */
	TSharedPtr<SButton>				KismetButton;

	/**	The color button for the Level */
	TSharedPtr<SButton>				ColorButton;
};
