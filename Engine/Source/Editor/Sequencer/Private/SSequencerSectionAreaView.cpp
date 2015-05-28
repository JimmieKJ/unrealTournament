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
		// Where to start drawing the section
		float StartX =  TimeToPixelConverter.TimeToPixel( Section->GetStartTime() );
		// Where to stop drawing the section
		float EndX = TimeToPixelConverter.TimeToPixel( Section->GetEndTime() );
		
		// Actual section length without grips.
		float SectionLengthActual = EndX-StartX;

		float SectionLengthWithGrips = SectionLengthActual+SequencerSectionConstants::SectionGripSize*2;

		float ActualHeight = NodeHeight / MaxTracks;

		// Compute allotted geometry area that can be used to draw the section
		return AllottedGeometry.MakeChild( FVector2D( StartX-SequencerSectionConstants::SectionGripSize, ActualHeight * RowIndex ), FVector2D( SectionLengthWithGrips, ActualHeight ) );
	}

}


void SSequencerSectionAreaView::Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node )
{
	SSequencerSectionAreaViewBase::Construct( SSequencerSectionAreaViewBase::FArguments(), Node );

	ViewRange = InArgs._ViewRange;

	check( Node->GetType() == ESequencerNode::Track );
	SectionAreaNode = StaticCastSharedRef<FTrackNode>( Node );

	SetVisibility( TAttribute<EVisibility>( this, &SSequencerSectionAreaView::GetNodeVisibility ) );

	BackgroundBrush = FEditorStyle::GetBrush("Sequencer.SectionArea.Background");

	// Generate widgets for sections in this view
	GenerateSectionWidgets();
}

void SSequencerSectionAreaView::GenerateSectionWidgets()
{
	Children.Empty();

	if( SectionAreaNode.IsValid() )
	{
		const TArray< TSharedRef<ISequencerSection> >& Sections = SectionAreaNode->GetSections();

		for ( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
		{
			Children.Add( 
				SNew( SSection, SectionAreaNode.ToSharedRef(), SectionIndex ) 
				.Visibility( this, &SSequencerSectionAreaView::GetSectionVisibility, Sections[SectionIndex]->GetSectionObject() )
				);
		}
	}
}

EVisibility SSequencerSectionAreaView::GetSectionVisibility( UMovieSceneSection* SectionObject ) const
{
	return GetSequencer().IsSectionVisible( SectionObject ) ? EVisibility::Visible : EVisibility::Collapsed;
}

float SSequencerSectionAreaView::GetSectionAreaHeight() const
{
 	return SectionAreaNode->GetNodeHeight() + ComputeHeightRecursive( SectionAreaNode->GetChildNodes() );
}

EVisibility SSequencerSectionAreaView::GetNodeVisibility() const
{
	// We are not visible if our node is hidden
	return SectionAreaNode->IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}


/** SWidget Interface */
int32 SSequencerSectionAreaView::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	if( SectionAreaNode.IsValid() )
	{
		// Draw a region around the entire section area
		FSlateDrawElement::MakeBox( 
			OutDrawElements, 
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			BackgroundBrush,
			MyClippingRect,
			ESlateDrawEffect::None,
			SequencerSectionAreaConstants::BackgroundColor
			);
	}

	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
		FSlateRect ChildClipRect = MyClippingRect.IntersectionWith( CurWidget.Geometry.GetClippingRect() );
		const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint( Args.WithNewParent(this), CurWidget.Geometry, ChildClipRect, OutDrawElements, LayerId+1, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}

	return LayerId+1;
}

FReply SSequencerSectionAreaView::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Clear selected sections
	if( !MouseEvent.IsControlDown() )
	{
		GetSequencer().GetSelection()->EmptySelectedSections();
	}

	return FReply::Handled();
}

void SSequencerSectionAreaView::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	int32 MaxRowIndex = 0;
	for( int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex )
	{
		const TSharedRef<SSection>& Widget = Children[WidgetIndex];

		TSharedPtr<ISequencerSection> SectionInterface = Widget->GetSectionInterface();

		MaxRowIndex = FMath::Max(MaxRowIndex, SectionInterface->GetSectionObject()->GetRowIndex());
	}
	int32 MaxTracks = MaxRowIndex + 1;


	float SectionHeight = GetSectionAreaHeight();

	FTimeToPixel TimeToPixelConverter = GetTimeToPixel( AllottedGeometry );

	for( int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex )
	{
		const TSharedRef<SSection>& Widget = Children[WidgetIndex];

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