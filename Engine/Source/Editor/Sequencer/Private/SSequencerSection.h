// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SequencerSelectedKey.h"
#include "Rendering/RenderingCommon.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "SectionLayout.h"

class FPaintArgs;
class FSequencer;
class FSequencerSectionPainter;
class FSlateWindowElementList;
struct ISequencerHotspot;

class SSequencerSection : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSequencerSection )
	{}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencerTrackNode> SectionNode, int32 SectionIndex );

	TSharedPtr<ISequencerSection> GetSectionInterface() const { return SectionInterface; }

	/** Caches the parent geometry to be given to section interfaces that need it on tick */
	void CacheParentGeometry(const FGeometry& InParentGeometry) { ParentGeometry = InParentGeometry; }

	virtual FVector2D ComputeDesiredSize(float) const override;

private:

	/**
	 * Computes the geometry for a key area
	 *
	 * @param KeyArea			The key area to compute geometry for
	 * @param SectionGeometry	The geometry of the section
	 * @return The geometry of the key area
	 */
	FGeometry GetKeyAreaGeometry( const FSectionLayoutElement& KeyArea, const FGeometry& SectionGeometry ) const;

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
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;

	/**
	 * Paints keys visible inside the section
	 *
	 * @param InPainter			Section painter
	 */
	void PaintKeys( FSequencerSectionPainter& InPainter, const FWidgetStyle& InWidgetStyle, const FSlateRect& KeyClippingRect ) const;

	/**
	 * Draw the section resize handles.
	 */
	void DrawSectionHandles( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect::Type DrawEffects, FLinearColor SelectionColor, const ISequencerHotspot* Hotspot ) const;

	/** @return the sequencer interface */
	FSequencer& GetSequencer() const;

	/** 
	 * Creates geometry for a section without space for the handles
	 */
	FGeometry MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const;


public:

	/** Indicate that the current key selection should throb the specified number of times. A single throb takes 0.2s. */
	static void ThrobSelection(int32 ThrobCount = 1);

	/** Get a value between 0 and 1 that indicicates the amount of throb-scale to apply to the currently selected keys */
	static float GetSelectionThrobValue();

private:
	/** Interface to section data */
	TSharedPtr<ISequencerSection> SectionInterface;
	/** Section area where this section resides */
	TSharedPtr<FSequencerTrackNode> ParentSectionArea;
	/** Cached layout generated each tick */
	TOptional<FSectionLayout> Layout;
	/** The index of this section in the parent section area */
	int32 SectionIndex;
	/** Cached parent geometry to pass down to any section interfaces that need it during tick */
	FGeometry ParentGeometry;
	/** The end time for a throbbing animation for selected keys */
	static double SelectionThrobEndTime;
	/** Handle offset amount in pixels */
	float HandleOffsetPx;
};
