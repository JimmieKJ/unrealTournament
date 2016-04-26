// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "Sequencer2DTransformTrackEditor.h"
#include "MovieScene2DTransformSection.h"
#include "MovieScene2DTransformTrack.h"
#include "PropertySection.h"
#include "ISectionLayoutBuilder.h"
#include "FloatCurveKeyArea.h"


FName F2DTransformTrackEditor::TranslationName( "Translation" );
FName F2DTransformTrackEditor::ScaleName( "Scale" );
FName F2DTransformTrackEditor::ShearName( "Shear" );
FName F2DTransformTrackEditor::AngleName( "Angle" );


class F2DTransformSection
	: public FPropertySection
{
public:

	F2DTransformSection( UMovieSceneSection& InSectionObject, const FText& SectionName )
		: FPropertySection(InSectionObject, SectionName) {}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		static const FLinearColor GreenKeyAreaColor(0.0f, 0.7f, 0.0f, 0.2f);
		static const FLinearColor RedKeyAreaColor(0.7f, 0.0f, 0.0f, 0.2f);

		UMovieScene2DTransformSection* TransformSection = Cast<UMovieScene2DTransformSection>(&SectionObject);

		TranslationXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::X), TransformSection, RedKeyAreaColor));
		TranslationYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::Y), TransformSection, GreenKeyAreaColor));

		RotationKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetRotationCurve(), TransformSection));

		ScaleXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::X), TransformSection, RedKeyAreaColor));
		ScaleYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::Y), TransformSection, GreenKeyAreaColor));

		ShearXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetSheerCurve(EAxis::X), TransformSection, RedKeyAreaColor));
		ShearYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetSheerCurve(EAxis::Y), TransformSection, GreenKeyAreaColor));

		// This generates the tree structure for the transform section
		LayoutBuilder.PushCategory("Translation", NSLOCTEXT("F2DTransformSection", "TranslationArea", "Translation"));
			LayoutBuilder.AddKeyArea("Translation.X", NSLOCTEXT("F2DTransformSection", "TransXArea", "X"), TranslationXKeyArea.ToSharedRef());
			LayoutBuilder.AddKeyArea("Translation.Y", NSLOCTEXT("F2DTransformSection", "TransYArea", "Y"), TranslationYKeyArea.ToSharedRef());
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Rotation", NSLOCTEXT("F2DTransformSection", "RotationArea", "Rotation"));
			LayoutBuilder.AddKeyArea("Rotation.Angle", NSLOCTEXT("F2DTransformSection", "AngleArea", "Angle"), RotationKeyArea.ToSharedRef());
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Scale", NSLOCTEXT("F2DTransformSection", "ScaleArea", "Scale"));
			LayoutBuilder.AddKeyArea("Scale.X", NSLOCTEXT("F2DTransformSection", "ScaleXArea", "X"), ScaleXKeyArea.ToSharedRef());
			LayoutBuilder.AddKeyArea("Scale.Y", NSLOCTEXT("F2DTransformSection", "ScaleYArea", "Y"), ScaleYKeyArea.ToSharedRef());
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Shear", NSLOCTEXT("F2DTransformSection", "ShearArea", "Shear"));
			LayoutBuilder.AddKeyArea("Shear.X", NSLOCTEXT("F2DTransformSection", "SheerXArea", "X"), ShearXKeyArea.ToSharedRef());
			LayoutBuilder.AddKeyArea("Shear.Y", NSLOCTEXT("F2DTransformSection", "SheerYArea", "Y"), ShearYKeyArea.ToSharedRef());
		LayoutBuilder.PopCategory();
	}

	virtual void SetIntermediateValue( FPropertyChangedParams PropertyChangedParams ) override
	{
		FWidgetTransform Transform = PropertyChangedParams.GetPropertyValue<FWidgetTransform>();

		TranslationXKeyArea->SetIntermediateValue(Transform.Translation.X);
		TranslationYKeyArea->SetIntermediateValue(Transform.Translation.Y);

		RotationKeyArea->SetIntermediateValue(Transform.Angle);

		ScaleXKeyArea->SetIntermediateValue(Transform.Scale.X);
		ScaleYKeyArea->SetIntermediateValue(Transform.Scale.Y);

		ShearXKeyArea->SetIntermediateValue(Transform.Shear.X);
		ShearYKeyArea->SetIntermediateValue(Transform.Shear.Y);
	}

	virtual void ClearIntermediateValue() override
	{
		TranslationXKeyArea->ClearIntermediateValue();
		TranslationYKeyArea->ClearIntermediateValue();

		RotationKeyArea->ClearIntermediateValue();

		ScaleXKeyArea->ClearIntermediateValue();
		ScaleYKeyArea->ClearIntermediateValue();

		ShearXKeyArea->ClearIntermediateValue();
		ShearYKeyArea->ClearIntermediateValue();
	}

private:
	mutable TSharedPtr<FFloatCurveKeyArea> TranslationXKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> TranslationYKeyArea;

	mutable TSharedPtr<FFloatCurveKeyArea> ScaleXKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> ScaleYKeyArea;

	mutable TSharedPtr<FFloatCurveKeyArea> ShearXKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> ShearYKeyArea;

	mutable TSharedPtr<FFloatCurveKeyArea> RotationKeyArea;
};


TSharedRef<ISequencerTrackEditor> F2DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F2DTransformTrackEditor( InSequencer ) );
}


TSharedRef<FPropertySection> F2DTransformTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	UClass* SectionClass = SectionObject.GetOuter()->GetClass();
	return MakeShareable(new F2DTransformSection(SectionObject, Track.GetDisplayName()));
}


void F2DTransformTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<F2DTransformKey>& NewGeneratedKeys, TArray<F2DTransformKey>& DefaultGeneratedKeys )
{
	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;
	FWidgetTransform Transform = PropertyChangedParams.GetPropertyValue<FWidgetTransform>();

	TArray<F2DTransformKey>& TranslationKeys = ChannelName == NAME_None || ChannelName == TranslationName ? NewGeneratedKeys : DefaultGeneratedKeys;
	TranslationKeys.Add( F2DTransformKey( EKey2DTransformChannel::Translation, EKey2DTransformAxis::X, Transform.Translation.X ) );
	TranslationKeys.Add( F2DTransformKey( EKey2DTransformChannel::Translation, EKey2DTransformAxis::Y, Transform.Translation.Y ) );

	TArray<F2DTransformKey>& ScaleKeys = ChannelName == NAME_None || ChannelName == ScaleName ? NewGeneratedKeys : DefaultGeneratedKeys;
	ScaleKeys.Add( F2DTransformKey( EKey2DTransformChannel::Scale, EKey2DTransformAxis::X, Transform.Scale.X ) );
	ScaleKeys.Add( F2DTransformKey( EKey2DTransformChannel::Scale, EKey2DTransformAxis::Y, Transform.Scale.Y ) );

	TArray<F2DTransformKey>& ShearKeys = ChannelName == NAME_None || ChannelName == ShearName ? NewGeneratedKeys : DefaultGeneratedKeys;
	ShearKeys.Add( F2DTransformKey( EKey2DTransformChannel::Shear, EKey2DTransformAxis::X, Transform.Shear.X ) );
	ShearKeys.Add( F2DTransformKey( EKey2DTransformChannel::Shear, EKey2DTransformAxis::Y, Transform.Shear.Y ) );

	TArray<F2DTransformKey>& AngleKeys = ChannelName == NAME_None || ChannelName == AngleName ? NewGeneratedKeys : DefaultGeneratedKeys;
	AngleKeys.Add( F2DTransformKey( EKey2DTransformChannel::Rotation, EKey2DTransformAxis::None, Transform.Angle ) );
}
