// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneColorTrack.h"
#include "MovieSceneColorSection.h"
#include "ColorPropertySection.h"
#include "CommonMovieSceneTools.h"
#include "MovieSceneSequence.h"
#include "FloatCurveKeyArea.h"


void FColorPropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>( &SectionObject );

	RedKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetRedCurve(), ColorSection ) ) ;
	GreenKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetGreenCurve(), ColorSection ) ) ;
	BlueKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetBlueCurve(), ColorSection ) ) ;
	AlphaKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetAlphaCurve(), ColorSection ) );

	LayoutBuilder.AddKeyArea( "R", NSLOCTEXT( "FColorPropertySection", "RedArea", "Red" ), RedKeyArea.ToSharedRef() );
	LayoutBuilder.AddKeyArea( "G", NSLOCTEXT( "FColorPropertySection", "GreenArea", "Green" ), GreenKeyArea.ToSharedRef() );
	LayoutBuilder.AddKeyArea( "B", NSLOCTEXT( "FColorPropertySection", "BlueArea", "Blue" ), BlueKeyArea.ToSharedRef() );
	LayoutBuilder.AddKeyArea( "A", NSLOCTEXT( "FColorPropertySection", "OpacityArea", "Opacity" ), AlphaKeyArea.ToSharedRef() );
}


int32 FColorPropertySection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	const ESlateDrawEffect::Type DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const UMovieSceneColorSection* ColorSection = Cast<const UMovieSceneColorSection>( &SectionObject );

	const FTimeToPixel& TimeConverter = Painter.GetTimeConverter();

	const float StartTime = TimeConverter.PixelToTime(0.f);
	const float EndTime = TimeConverter.PixelToTime(Painter.SectionGeometry.GetLocalSize().X);
	const float SectionDuration = EndTime - StartTime;

	if ( !FMath::IsNearlyZero( SectionDuration ) )
	{
		FVector2D GradientSize = FVector2D( Painter.SectionGeometry.Size.X - 2.f, (Painter.SectionGeometry.Size.Y / 4) - 3.0f );

		FPaintGeometry PaintGeometry = Painter.SectionGeometry.ToPaintGeometry( FVector2D( 1.f, 1.f ), GradientSize );
		FSlateRect ClippingRect = Painter.SectionClippingRect.InsetBy(1.f);

		// If we are showing a background pattern and the colors is transparent, draw a checker pattern
		FSlateDrawElement::MakeBox(
			Painter.DrawElements,
			LayerId,
			PaintGeometry,
			FEditorStyle::GetBrush( "Checker" ),
			ClippingRect,
			DrawEffects);

		TArray<FSlateGradientStop> GradientStops;

		TArray< TKeyValuePair<float, FLinearColor> > ColorKeys;
		ConsolidateColorCurves( ColorKeys, ColorSection );

		for ( int32 i = 0; i < ColorKeys.Num(); ++i )
		{
			float Time = ColorKeys[i].Key;
			FLinearColor Color = ColorKeys[i].Value;

			// HACK: The color is converted to SRgb and then reinterpreted as linear here because gradients are converted to FColor
			// without the SRgb conversion before being passed to the renderer for some reason.
			Color = Color.ToFColor( true ).ReinterpretAsLinear();

			float TimeFraction = (Time - StartTime) / SectionDuration;

			GradientStops.Add( FSlateGradientStop( FVector2D( TimeFraction * Painter.SectionGeometry.Size.X, 0 ),
				Color ) );
		}

		if ( GradientStops.Num() > 0 )
		{
			FSlateDrawElement::MakeGradient(
				Painter.DrawElements,
				Painter.LayerId + 1,
				PaintGeometry,
				GradientStops,
				Orient_Vertical,
				ClippingRect,
				DrawEffects
				);
		}
	}

	return LayerId + 1;
}


void FColorPropertySection::ConsolidateColorCurves( TArray< TKeyValuePair<float, FLinearColor> >& OutColorKeys, const UMovieSceneColorSection* Section ) const
{
	// Get the default color of the first instance
	static const FName SlateColorName("SlateColor");
	FLinearColor DefaultColor = FindSlateColor(SlateColorName);

	// @todo Sequencer Optimize - This could all get cached, instead of recalculating everything every OnPaint

	const FRichCurve* Curves[4] = {
		&Section->GetRedCurve(),
		&Section->GetGreenCurve(),
		&Section->GetBlueCurve(),
		&Section->GetAlphaCurve()
	};

	// @todo Sequencer Optimize - This is a O(n^2) loop!
	// Our times are floats, which means we can't use a map and
	// do a quick lookup to see if the keys already exist
	// because the keys are ordered, we could take advantage of that, however
	TArray<float> TimesWithKeys;
	for ( int32 i = 0; i < 4; ++i )
	{
		const FRichCurve* Curve = Curves[i];
		for ( auto It( Curve->GetKeyIterator() ); It; ++It )
		{
			float KeyTime = It->Time;

			bool bShouldAddKey = true;

			int32 InsertKeyIndex = INDEX_NONE;
			for ( int32 k = 0; k < TimesWithKeys.Num(); ++k )
			{
				if ( FMath::IsNearlyEqual( TimesWithKeys[k], KeyTime ) )
				{
					bShouldAddKey = false;
					break;
				}
				else if ( TimesWithKeys[k] > KeyTime )
				{
					InsertKeyIndex = k;
					break;
				}
			}

			if ( InsertKeyIndex == INDEX_NONE && bShouldAddKey )
			{
				InsertKeyIndex = TimesWithKeys.Num();
			}

			if ( bShouldAddKey )
			{
				TimesWithKeys.Insert( KeyTime, InsertKeyIndex );
			}
		}
	}

	// Enforce at least one key for the default value
	if (TimesWithKeys.Num() == 0)
	{
		TimesWithKeys.Add(0);
	}

	// @todo Sequencer Optimize - This another O(n^2) loop, since Eval is O(n)!
	for ( int32 i = 0; i < TimesWithKeys.Num(); ++i )
	{
		OutColorKeys.Add( TKeyValuePair<float, FLinearColor>( TimesWithKeys[i], Section->Eval( TimesWithKeys[i], DefaultColor ) ) );
	}
}


FLinearColor FColorPropertySection::FindSlateColor(const FName& ColorName) const
{
	const UMovieSceneSequence* FocusedSequence = Sequencer->GetFocusedMovieSceneSequence();
	const TArray<FMovieSceneBinding>& FocusedBindings = FocusedSequence->GetMovieScene()->GetBindings();

	for (const FMovieSceneBinding& Binding : FocusedBindings)
	{
		for (const UMovieSceneTrack* BindingTrack : Binding.GetTracks())
		{
			if (BindingTrack != &Track)
			{
				continue;
			}

			UObject* RuntimeObject = Sequencer->GetFocusedMovieSceneSequenceInstance()->FindObject(Binding.GetObjectGuid(), *Sequencer);

			if (RuntimeObject == nullptr)
			{
				continue;
			}

			UProperty* Property = RuntimeObject->GetClass()->FindPropertyByName(Track.GetPropertyName());
			UStructProperty* ColorStructProp = Cast<UStructProperty>(Property);
			
			if ((ColorStructProp == nullptr) || (ColorStructProp->Struct == nullptr))
			{
				continue;
			}

			if (ColorStructProp->Struct->GetFName() == ColorName)
			{
				return (*Property->ContainerPtrToValuePtr<FSlateColor>(RuntimeObject)).GetSpecifiedColor();
			}

			if (ColorStructProp->Struct->GetFName() == NAME_LinearColor)
			{
				return *Property->ContainerPtrToValuePtr<FLinearColor>(RuntimeObject);
			}

			return *Property->ContainerPtrToValuePtr<FColor>(RuntimeObject);
		}
	}

	return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
}


void FColorPropertySection::SetIntermediateValue( FPropertyChangedParams PropertyChangedParams )
{
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.Last() );
	FName StructName = StructProp->Struct->GetFName();
	bool bIsFColor = StructName == NAME_Color;
	bool bIsFLinearColor = StructName == NAME_LinearColor;
	bool bIsSlateColor = StructName == FName( "SlateColor" );

	FLinearColor ColorValue;

	if ( bIsFColor )
	{
		ColorValue = PropertyChangedParams.GetPropertyValue<FColor>();
	}
	else
	{
		ColorValue = PropertyChangedParams.GetPropertyValue<FLinearColor>();
	}

	// @todo - Sequencer - This will cause slightly different values to show up in the key editors for FColors.
	RedKeyArea->SetIntermediateValue(ColorValue.R);
	GreenKeyArea->SetIntermediateValue(ColorValue.G);
	BlueKeyArea->SetIntermediateValue(ColorValue.B);
	AlphaKeyArea->SetIntermediateValue(ColorValue.A);
}


void FColorPropertySection::ClearIntermediateValue()
{
	RedKeyArea->ClearIntermediateValue();
	GreenKeyArea->ClearIntermediateValue();
	BlueKeyArea->ClearIntermediateValue();
	AlphaKeyArea->ClearIntermediateValue();
}
