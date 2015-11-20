// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneBoolTrack.h"
#include "MovieSceneBoolSection.h"
#include "BoolPropertySection.h"

void FBoolPropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>( &SectionObject );
	KeyArea = MakeShareable( new FBoolKeyArea( BoolSection->GetCurve(), BoolSection ) );
	LayoutBuilder.SetSectionAsKeyArea( KeyArea.ToSharedRef() );
}

int32 FBoolPropertySection::OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const
{
	const ESlateDrawEffect::Type DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	FTimeToPixel TimeToPixelConverter = SectionObject.IsInfinite() ?
		FTimeToPixel( AllottedGeometry, Sequencer->GetViewRange() ) :
		FTimeToPixel( AllottedGeometry, TRange<float>( SectionObject.GetStartTime(), SectionObject.GetEndTime() ) );

	// custom drawing for bool curves
	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>( &SectionObject );

	TArray<float> SectionSwitchTimes;

	if ( SectionObject.IsInfinite() && Sequencer->GetViewRange().GetLowerBoundValue() < SectionObject.GetStartTime() )
	{
		SectionSwitchTimes.Add( Sequencer->GetViewRange().GetLowerBoundValue() );
	}
	SectionSwitchTimes.Add( SectionObject.GetStartTime() );

	FIntegralCurve& BoolCurve = BoolSection->GetCurve();
	for ( TArray<FIntegralKey>::TConstIterator It( BoolCurve.GetKeyIterator() ); It; ++It )
	{
		float Time = It->Time;
		if ( Time > SectionObject.GetStartTime() && Time < SectionObject.GetEndTime() )
		{
			SectionSwitchTimes.Add( Time );
		}
	}

	if ( SectionObject.IsInfinite() && Sequencer->GetViewRange().GetUpperBoundValue() > SectionObject.GetEndTime() )
	{
		SectionSwitchTimes.Add( Sequencer->GetViewRange().GetUpperBoundValue() );
	}
	SectionSwitchTimes.Add( SectionObject.GetEndTime() );

	for ( int32 i = 0; i < SectionSwitchTimes.Num() - 1; ++i )
	{
		float FirstTime = SectionSwitchTimes[i];
		float NextTime = SectionSwitchTimes[i + 1];
		float FirstTimeInPixels = TimeToPixelConverter.TimeToPixel( FirstTime );
		float NextTimeInPixels = TimeToPixelConverter.TimeToPixel( NextTime );
		if ( BoolSection->Eval( FirstTime ) )
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( FirstTimeInPixels, 0 ), FVector2D( NextTimeInPixels - FirstTimeInPixels, AllottedGeometry.Size.Y ) ),
				FEditorStyle::GetBrush( "Sequencer.GenericSection.Background" ),
				SectionClippingRect,
				DrawEffects,
				FLinearColor( 0.f, 1.f, 0.f, 1.f )
				);
		}
		else
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( FirstTimeInPixels, 0 ), FVector2D( NextTimeInPixels - FirstTimeInPixels, AllottedGeometry.Size.Y ) ),
				FEditorStyle::GetBrush( "Sequencer.GenericSection.Background" ),
				SectionClippingRect,
				DrawEffects,
				FLinearColor( 1.f, 0.f, 0.f, 1.f )
				);
		}
	}

	return LayerId + 1;
}

void FBoolPropertySection::SetIntermediateValue( FPropertyChangedParams PropertyChangedParams )
{
	KeyArea->SetIntermediateValue( PropertyChangedParams.GetPropertyValue<bool>() );
}

void FBoolPropertySection::ClearIntermediateValue()
{
	KeyArea->ClearIntermediateValue();
}