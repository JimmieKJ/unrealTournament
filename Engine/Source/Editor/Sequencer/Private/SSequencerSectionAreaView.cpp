// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SSequencerSectionAreaView.h"
#include "IKeyArea.h"
#include "ISequencerSection.h"
#include "MovieSceneSection.h"
#include "SequencerDisplayNode.h"
#include "SSequencer.h"
#include "Sequencer.h"
#include "MovieSceneShotSection.h"
#include "CommonMovieSceneTools.h"

namespace SequencerSectionAreaConstants
{

	/** Background color of section areas */
	const FLinearColor BackgroundColor( .1f, .1f, .1f, 0.5f );
}

namespace SequencerSectionUtils
{
	/**
	 * Gets the geometry of a section, optionally inflated by some margin
	 *
	 * @param AllottedGeometry		The geometry of the area where sections are located
	 * @param NodeHeight			The height of the section area (and its children)
	 * @param SectionInterface		Interface to the section to get geometry for
	 * @param TimeToPixelConverter  Converts time to pixels and vice versa
	 */
	FGeometry GetSectionGeometry( const FGeometry& AllottedGeometry, int32 RowIndex, int32 MaxTracks, float NodeHeight, TSharedPtr<ISequencerSection> SectionInterface, const FTimeToPixel& TimeToPixelConverter )
	{
		const UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		float PixelStartX = TimeToPixelConverter.TimeToPixel( Section->GetStartTime() );
		// Note the -1 pixel at the end is because the section does not actually end at the end time if there is a section starting at that same time.  It is more important that a section lines up correctly with it's true start time
		float PixelEndX = TimeToPixelConverter.TimeToPixel( Section->GetEndTime() );

		// If the section is infinite, occupy the entire width of the geometry where the section is located.
		if (Section->IsInfinite())
		{
			PixelStartX = AllottedGeometry.Position.X;
			PixelEndX = AllottedGeometry.Position.X + AllottedGeometry.Size.X;
		}

		// Actual section length without grips.
		float SectionLengthActual = FMath::Max(1.0f,PixelEndX-PixelStartX);

		float SectionLengthWithGrips = SectionLengthActual;
		float ExtentSize = 0;
		if( !SectionInterface->AreSectionsConnected() )
		{
			// Extend the section to include areas for the grips
			ExtentSize = SectionInterface->GetSectionGripSize();
			// Connected sections do not display grips outside of their section area
			SectionLengthWithGrips += (ExtentSize*2);
		}

		float ActualHeight = NodeHeight / MaxTracks;

		// Compute allotted geometry area that can be used to draw the section
		return AllottedGeometry.MakeChild( FVector2D( PixelStartX-ExtentSize, ActualHeight * RowIndex ), FVector2D( SectionLengthWithGrips, ActualHeight ) );
	}

}


void SSequencerSectionAreaView::Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node )
{
	//SSequencerSectionAreaViewBase::Construct( SSequencerSectionAreaViewBase::FArguments(), Node );

	ViewRange = InArgs._ViewRange;

	check( Node->GetType() == ESequencerNode::Track );
	SectionAreaNode = StaticCastSharedRef<FSequencerTrackNode>( Node );

	BackgroundBrush = FEditorStyle::GetBrush("Sequencer.SectionArea.Background");

	// Generate widgets for sections in this view
	GenerateSectionWidgets();
};

FVector2D SSequencerSectionAreaView::ComputeDesiredSize(float) const
{
	// Note: X Size is not used
	FVector2D Size(100, 0.f);
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		Size.Y = FMath::Max(Size.Y, Children[Index]->GetDesiredSize().Y);
	}
	return Size + SectionAreaNode->GetNodePadding().Bottom;
}

void SSequencerSectionAreaView::GenerateSectionWidgets()
{
	Children.Empty();

	if( SectionAreaNode.IsValid() )
	{
		TArray< TSharedRef<ISequencerSection> >& Sections = SectionAreaNode->GetSections();

		for ( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
		{
			Children.Add( 
				SNew( SSequencerSection, SectionAreaNode.ToSharedRef(), SectionIndex ) 
				.Visibility( this, &SSequencerSectionAreaView::GetSectionVisibility, Sections[SectionIndex]->GetSectionObject() )
				);
		}
	}
}

EVisibility SSequencerSectionAreaView::GetSectionVisibility( UMovieSceneSection* SectionObject ) const
{
	return EVisibility::Visible;
}


/** SWidget Interface */
int32 SSequencerSectionAreaView::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
		FSlateRect ChildClipRect = MyClippingRect.IntersectionWith( CurWidget.Geometry.GetClippingRect() );
		const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint( Args.WithNewParent(this), CurWidget.Geometry, ChildClipRect, OutDrawElements, LayerId+1, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}

	return LayerId+1;
}

void SSequencerSectionAreaView::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	int32 MaxRowIndex = 0;
	for( int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex )
	{
		const TSharedRef<SSequencerSection>& Widget = Children[WidgetIndex];

		TSharedPtr<ISequencerSection> SectionInterface = Widget->GetSectionInterface();

		MaxRowIndex = FMath::Max(MaxRowIndex, SectionInterface->GetSectionObject()->GetRowIndex());
	}
	int32 MaxTracks = MaxRowIndex + 1;


	float SectionHeight = AllottedGeometry.GetLocalSize().Y - SectionAreaNode->GetNodePadding().Bottom;

	FTimeToPixel TimeToPixelConverter = GetTimeToPixel( AllottedGeometry );

	for( int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex )
	{
		const TSharedRef<SSequencerSection>& Widget = Children[WidgetIndex];

		TSharedPtr<ISequencerSection> SectionInterface = Widget->GetSectionInterface();

		int32 RowIndex = SectionInterface->GetSectionObject()->GetRowIndex();

		FGeometry SectionGeometry = SequencerSectionUtils::GetSectionGeometry( AllottedGeometry, RowIndex, MaxTracks, SectionHeight, SectionInterface, TimeToPixelConverter );

		EVisibility Visibility = Widget->GetVisibility();
		if( ArrangedChildren.Accepts( Visibility ) )
		{
			Widget->CacheParentGeometry(AllottedGeometry);

			ArrangedChildren.AddWidget( 
				Visibility, 
				AllottedGeometry.MakeChild( Widget, SectionGeometry.Position, SectionGeometry.GetDrawSize() )
				);
		}
	}
}

FTimeToPixel SSequencerSectionAreaView::GetTimeToPixel( const FGeometry& AllottedGeometry ) const
{
	return FTimeToPixel( AllottedGeometry, ViewRange.Get() );
}


FSequencer& SSequencerSectionAreaView::GetSequencer() const
{
	return SectionAreaNode->GetSequencer();
}