// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IKeyArea;

namespace SequencerSectionConstants
{
	/** How far the user has to drag the mouse before we consider the action dragging rather than a click */
	const float SectionDragStartDistance = 5.0f;

	/** The size of each key */
	const FVector2D KeySize(11.0f, 11.0f);

	const float DefaultSectionGripSize = 7.0f;

	const float DefaultSectionHeight = 15.f;

	const FName SelectionColorName("SelectionColor");

	const FName SelectionInactiveColorName("SelectionColorInactive");
	
	const FName DefaultSectionGripLeftImageName("Sequencer.DefaultSectionGripLeft");
	
	const FName DefaultSectionGripRightImageName("Sequencer.DefaultSectionGripRight");
}

/**
 * Interface that should be implemented for the UI portion of a section
 */
class ISequencerSection
{
public:
	virtual ~ISequencerSection(){}
	/**
	 * The MovieSceneSection data being visualized
	 */
	virtual UMovieSceneSection* GetSectionObject() = 0;

	/**
	 * Allows each section to have it's own unique widget for advanced editing functionality
	 * OnPaintSection will still be called if a widget is provided.  OnPaintSection is still used for the background section display
	 * 
	 * @return The generated widget 
	 */
	virtual TSharedRef<SWidget> GenerateSectionWidget() { return SNullWidget::NullWidget; }

	/**
	 * Called when the section should be painted
	 *
	 * @param AllottedGeometry		The geometry of the section
	 * @param SectionClippingRect	The clipping rect around the section
	 * @param OutDrawElements		The element list ot add elements to
	 * @param LayerId				The ID of the starting layer to draw elements to
	 * @param bParentEnabled		Whether or not the parent widget is enabled
	 * @return						The new LayerId
	 */
	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const = 0;

	/** Allows a section to override the brush to use for a key by handle.
	 *
	 * @param KeyHandle the handle of the key to get a brush for.
	 * @return A const pointer to a slate brush if the brush should be overridden, otherwise null.
	 */
	virtual const FSlateBrush* GetKeyBrush(FKeyHandle KeyHandle) const { return nullptr; }

	/**
	 * Called when the section is double clicked
	 *
	 * @param SectionGeometry	Geometry of the section
	 * @param MouseEvent		Event causing the double click
	 * @return A reply in response to double clicking the section
	 */
	virtual FReply OnSectionDoubleClicked( const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent ) { return FReply::Unhandled(); }

	/**
	 * @return The display name of the section for the animation outliner
	 */
	virtual FText GetDisplayName() const = 0;
	
	/**
	 * @return The display name of the section in the section view
	 */
	virtual FText GetSectionTitle() const = 0;

	/**
	 * Generates the inner layout for this section
	 *
	 * @param LayoutBuilder	The builder utility for creating section layouts
	 */	
	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const = 0;

	/**
	 * @return The height of the section
	 */
	virtual float GetSectionHeight() const { return SequencerSectionConstants::DefaultSectionHeight; }
	
	virtual float GetSectionGripSize() const { return SequencerSectionConstants::DefaultSectionGripSize; }

	virtual FName GetSectionGripLeftBrushName() const { return SequencerSectionConstants::DefaultSectionGripLeftImageName; }

	virtual FName GetSectionGripRightBrushName() const { return SequencerSectionConstants::DefaultSectionGripRightImageName; }

	/**
	 * @return Whether or not the user can resize this section.
	 */
	virtual bool SectionIsResizable() const {return true;}


	virtual bool AreSectionsConnected() const { return false; }

	/**
	 * Ticks the section during the Slate tick
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  ClippedGeometry The space for this widget clipped against the parent widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime ) {}

	/**
	 * Builds up the section context menu for the outliner
	 *
	 * @param MenuBuilder	The menu builder to change
	 */
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder) {}

	/**
	 * Called when the user requests that a category from this section be deleted. 
	 *
	 * @param CategoryNamePath An array of category names which is the path to the category to be deleted.
	 * @returns Whether or not the category was deleted.
	 */
	virtual bool RequestDeleteCategory( const TArray<FName>& CategoryNamePath ) { return false; }

	/**
	* Called when the user requests that a key area from this section be deleted.
	*
	* @param KeyAreaNamePath An array of names representing the path of to the key area to delete, starting with any categories which contain the key area.
	* @returns Whether or not the key area was deleted.
	*/
	virtual bool RequestDeleteKeyArea( const TArray<FName>& KeyAreaNamePath ) { return false; }
};
