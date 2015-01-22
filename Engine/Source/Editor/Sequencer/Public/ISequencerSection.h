// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IKeyArea;

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
	virtual float GetSectionHeight() const { return 15.0f; }
	
	/**
	 * @return Whether or not the user can resize this section.
	 */
	virtual bool SectionIsResizable() const {return true;}

	/**
	 * Ticks the section during the Slate tick
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  ClippedGeometry The space for this widget clipped against the parent widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime ) {}
};
