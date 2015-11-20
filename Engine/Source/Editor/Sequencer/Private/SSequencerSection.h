// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyAreaLayout.h"


class SSequencerSection : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSequencerSection )
	{}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencerTrackNode> SectionNode, int32 SectionIndex );

	TSharedPtr<ISequencerSection> GetSectionInterface() const { return SectionInterface; }

	/** Caches the parent geometry to be given to section interfaces that need it on tick */
	void CacheParentGeometry(const FGeometry& InParentGeometry) {ParentGeometry = InParentGeometry;}

	virtual FVector2D ComputeDesiredSize(float) const override;
	
	/** Temporarily disable dynamic layout regeneration. This prevents overlapping key groups from being amalgamated during drags. Key times will continue to update correctly. */
	static void DisableLayoutRegeneration();

	/** Re-enable dynamic layout regeneration */
	static void EnableLayoutRegeneration();

private:

	/**
	 * Computes the geometry for a key area
	 *
	 * @param KeyArea			The key area to compute geometry for
	 * @param SectionGeometry	The geometry of the section
	 * @return The geometry of the key area
	 */
	FGeometry GetKeyAreaGeometry( const FKeyAreaLayoutElement& KeyArea, const FGeometry& SectionGeometry ) const;

	/**
	 * Determines the key that is under the mouse
	 *
	 * @param MousePosition		The current screen space position of the mouse
	 * @param AllottedGeometry	The geometry of the mouse event
	 * @return The key that is under the mouse.  Invalid if there is no key under the mouse
	 */
	FSequencerSelectedKey GetKeyUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry ) const;

	/**
	 * Creates a key at the mouse position
	 *
	 * @param MousePosition		The current screen space position of the mouse
	 * @param AllottedGeometry	The geometry of the mouse event
	 * @param InPressedKey      Key if pressed
	 * @return The newly created key
	 */
	FSequencerSelectedKey CreateKeyUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry, FSequencerSelectedKey InPressedKey );

	/**
	 * Checks for user interaction (via the mouse) with the left and right edge of a section
	 *
	 * @param MousePosition		The current screen space position of the mouse
	 * @param SectionGeometry	The geometry of the section
	 */
	void CheckForEdgeInteraction( const FPointerEvent& MousePosition, const FGeometry& SectionGeometry );

	/** SWidget interface */
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;

	/**
	 * Paints keys visible inside the section
	 *
	 * @param AllottedGeometry	The geometry of the section where keys are painted
	 * @param MyClippingRect	ClippingRect of the section
	 * @param OutDrawElements	List of draw elements to add to
	 * @param LayerId			The starting draw area
	 */
	void PaintKeys( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const;

	/**
	 * Draw the section resize handles.
	 */
	void DrawSectionHandles( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect::Type DrawEffects, FLinearColor SelectionColor ) const;

	/**
	 * Draw a box representing this section's selection
	 */
	void DrawSelectionBorder( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect::Type DrawEffects, FLinearColor SelectionColor ) const;

	/** Summons a context menu over the associated section */
	TSharedPtr<SWidget> OnSummonContextMenu( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

	/** Add edit menu for trim and split */
	void AddEditMenu(FMenuBuilder& MenuBuilder);

	/** Add extrapolation menu for pre and post infinity */
	void AddExtrapolationMenu(FMenuBuilder& MenuBuilder, bool bPreInfinity);

	/**
	 * Selects the section or keys if they are found under the mouse
	 *
	 * @param MyGeometry Geometry of the section
	 * @param MouseEvent	The mouse event that may cause selection
	 */
	void HandleSelection( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

	/**
	 * Selects a key if needed
	 *
	 * @param Key				The key to select
	 * @param MouseEvent		The mouse event that may have caused selection
	 * @param bSelectDueToDrag	If we are selecting the key due to a drag operation rather than it just being clicked
	 */
	void HandleKeySelection( const FSequencerSelectedKey& Key, const FPointerEvent& MouseEvent, bool bSelectDueToDrag );

	/**
	 * Selects the section if needed
	 *
	 * @param MouseEvent		The mouse event that may have caused selection
	 * @param bSelectDueToDrag	If we are selecting the key due to a drag operation rather than it just being clicked
	 */
	void HandleSectionSelection( const FPointerEvent& MouseEvent, bool bSelectDueToDrag );

	/** @return the sequencer interface */
	FSequencer& GetSequencer() const;

	/**
	 * Creates a drag operation based on the current mouse event and what is selected
	 *
	 * @param MyGeometry		Geometry of the section
	 * @param MouseEvent		The mouse event that caused the drag
	 * @param bKeysUnderMouse	true if there are any keys under the mouse
	 */
	void CreateDragOperation( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bKeysUnderMouse );

	/**
	 * Resets entire internal state of the widget
	 */
	void ResetState();

	/**
	 * Resets the hovered state of elements in the widget (section edges, keys)
	 */
	void ResetHoveredState();

	/** 
	 * Creates geometry for a section without space for the handles
	 */
	FGeometry MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const;

	/** 
	 * Select all keys within the section
	 */
	void SelectAllKeys();

	/**
	 * Are there keys to select? 
	 */
	bool CanSelectAllKeys() const;

	/**
	 * Trim section 
	 */
	void TrimSection(bool bTrimLeft);

	/**
	 * Split section
	 */
	void SplitSection();

	/**
	 * Is there a trimmable section?
	 */
	bool IsTrimmable() const;

	/**
	 * Set extrapolation mode
	 */
	void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity);

	/**
	 * Is extrapolation mode selected
	 */
	bool IsExtrapolationModeSelected(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) const;

	/**
	 * Section active/inactive toggle
	 */
	void ToggleSectionActive();
	bool IsToggleSectionActive() const;

	/*
	 * Delete section
	 */ 
	void DeleteSection();

private:
	/** Interface to section data */
	TSharedPtr<ISequencerSection> SectionInterface;
	/** Section area where this section resides */
	TSharedPtr<FSequencerTrackNode> ParentSectionArea;
	/** Cached layout generated each tick */
	TOptional<FKeyAreaLayout> Layout;
	/** The currently pressed key if any */
	FSequencerSelectedKey PressedKey;
	/** The current hovered key if any */
	FSequencerSelectedKey HoveredKey;
	/** The index of this section in the parent section area */
	int32 SectionIndex;
	/** Whether or not the left edge of the section is hovered */
	bool bLeftEdgeHovered;
	/** Whether or not the left edge of the section is pressed */
	bool bLeftEdgePressed;
	/** Whether or not the right edge of the section is hovered */
	bool bRightEdgeHovered;
	/** Whether or not the right edge of the section is pressed */
	bool bRightEdgePressed;
	/** Cached parent geometry to pass down to any section interfaces that need it during tick */
	FGeometry ParentGeometry;
};