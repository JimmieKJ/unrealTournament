// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "MovieSceneMarginTrack.h"
#include "MarginTrackEditor.h"
#include "MovieSceneMarginSection.h"
#include "PropertySection.h"
#include "ISectionLayoutBuilder.h"
#include "FloatCurveKeyArea.h"


FName FMarginTrackEditor::LeftName( "Left" );
FName FMarginTrackEditor::TopName( "Top" );
FName FMarginTrackEditor::RightName( "Right" );
FName FMarginTrackEditor::BottomName( "Bottom" );


class FMarginPropertySection
	: public FPropertySection
{
public:

	FMarginPropertySection( UMovieSceneSection& InSectionObject, const FText& SectionName )
		: FPropertySection(InSectionObject, SectionName) {}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		UMovieSceneMarginSection* MarginSection = Cast<UMovieSceneMarginSection>(&SectionObject);

		LeftKeyArea = MakeShareable(new FFloatCurveKeyArea(&MarginSection->GetLeftCurve(), MarginSection));
		TopKeyArea = MakeShareable(new FFloatCurveKeyArea(&MarginSection->GetTopCurve(), MarginSection));
		RightKeyArea = MakeShareable(new FFloatCurveKeyArea(&MarginSection->GetRightCurve(), MarginSection));
		BottomKeyArea = MakeShareable(new FFloatCurveKeyArea( &MarginSection->GetBottomCurve(), MarginSection));

		LayoutBuilder.AddKeyArea("Left", NSLOCTEXT("FMarginPropertySection", "MarginLeft", "Left"), LeftKeyArea.ToSharedRef());
		LayoutBuilder.AddKeyArea("Top", NSLOCTEXT("FMarginPropertySection", "MarginTop", "Top"), TopKeyArea.ToSharedRef());
		LayoutBuilder.AddKeyArea("Right", NSLOCTEXT("FMarginPropertySection", "MarginRight", "Right"), RightKeyArea.ToSharedRef());
		LayoutBuilder.AddKeyArea("Bottom", NSLOCTEXT("FMarginPropertySection", "MarginBottom", "Bottom"), BottomKeyArea.ToSharedRef());
	}

	virtual void SetIntermediateValue( FPropertyChangedParams PropertyChangedParams ) override
	{
		FMargin Margin = PropertyChangedParams.GetPropertyValue<FMargin>();
		LeftKeyArea->SetIntermediateValue( Margin.Left );
		TopKeyArea->SetIntermediateValue( Margin.Top );
		RightKeyArea->SetIntermediateValue( Margin.Right );
		BottomKeyArea->SetIntermediateValue( Margin.Bottom );
	}


	virtual void ClearIntermediateValue() override
	{
		LeftKeyArea->ClearIntermediateValue();
		TopKeyArea->ClearIntermediateValue();
		RightKeyArea->ClearIntermediateValue();
		BottomKeyArea->ClearIntermediateValue();
	}

private:
	mutable TSharedPtr<FFloatCurveKeyArea> LeftKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> TopKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> RightKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> BottomKeyArea;
};


TSharedRef<ISequencerTrackEditor> FMarginTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FMarginTrackEditor( InSequencer ) );
}


TSharedRef<FPropertySection> FMarginTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	UClass* SectionClass = SectionObject.GetOuter()->GetClass();
	return MakeShareable(new FMarginPropertySection(SectionObject, Track.GetDisplayName()));
}


void FMarginTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FMarginKey>& GeneratedKeys )
{
	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;
	FMargin Margin = PropertyChangedParams.GetPropertyValue<FMargin>();

	GeneratedKeys.Add( FMarginKey( EKeyMarginChannel::Left, Margin.Left, ChannelName == NAME_None || ChannelName == LeftName ? EKeyMarginValueType::Key : EKeyMarginValueType::Default ) );
	GeneratedKeys.Add( FMarginKey( EKeyMarginChannel::Top, Margin.Top, ChannelName == NAME_None || ChannelName == TopName ? EKeyMarginValueType::Key : EKeyMarginValueType::Default ) );
	GeneratedKeys.Add( FMarginKey( EKeyMarginChannel::Right, Margin.Right, ChannelName == NAME_None || ChannelName == RightName ? EKeyMarginValueType::Key : EKeyMarginValueType::Default ) );
	GeneratedKeys.Add( FMarginKey( EKeyMarginChannel::Bottom, Margin.Bottom, ChannelName == NAME_None || ChannelName == BottomName ? EKeyMarginValueType::Key : EKeyMarginValueType::Default ) );
}

bool FMarginTrackEditor::ShouldAddKey(UMovieSceneMarginTrack* InTrack, FMarginKey InKey, FKeyParams InKeyParams) const
{
	return FPropertyTrackEditor::ShouldAddKey(InTrack, InKey, InKeyParams) && InKey.ValueType == EKeyMarginValueType::Key;
}
