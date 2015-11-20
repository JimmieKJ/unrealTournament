// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class UMovieSceneEventSection;


/**
 * Event track section, which paints and ticks the appropriate section.
 */
class FEventTrackSection
	: public ISequencerSection
{
public:

	/**
	 * Creates and initializes a new instance
	 *
	 * @param InSection The section being visualized.
	 * @param InSequencer The sequencer that owns this section.
	 */
	FEventTrackSection(UMovieSceneSection& InSection, TSharedPtr<ISequencer> InSequencer);

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
//	virtual TSharedRef<SWidget> GenerateSectionWidget() override;
	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override;
//	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetSectionTitle() const override;
//	virtual float GetSectionHeight() const override;
//	virtual float GetSectionGripSize() const override;
//	virtual FName GetSectionGripLeftBrushName() const override;
//	virtual FName GetSectionGripRightBrushName() const override;
//	virtual bool AreSectionsConnected() const override { return true; }
	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override;
//	virtual FReply OnSectionDoubleClicked( const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent ) override;
//	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder) override;

private:

	/** The section being visualized. */
	UMovieSceneEventSection* Section;

	/** The parent sequencer we are a part of. */
	TWeakPtr<ISequencer> Sequencer;
};
