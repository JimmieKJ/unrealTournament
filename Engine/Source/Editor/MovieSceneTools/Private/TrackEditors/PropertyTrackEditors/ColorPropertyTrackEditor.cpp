// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneColorTrack.h"
#include "ColorPropertyTrackEditor.h"
#include "ColorPropertySection.h"


FName FColorPropertyTrackEditor::RedName( "R" );
FName FColorPropertyTrackEditor::GreenName( "G" );
FName FColorPropertyTrackEditor::BlueName( "B" );
FName FColorPropertyTrackEditor::AlphaName( "A" );


TSharedRef<ISequencerTrackEditor> FColorPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FColorPropertyTrackEditor(InSequencer));
}


TSharedRef<FPropertySection> FColorPropertyTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	return MakeShareable(new FColorPropertySection(SectionObject, GetSequencer().Get(), Track));
}


void FColorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FColorKey>& NewGeneratedKeys, TArray<FColorKey>& DefaultGeneratedKeys )
{
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.Last() );
	FName StructName = StructProp->Struct->GetFName();
	FName PropertyName = PropertyChangedParams.PropertyPath.Last()->GetFName();

	bool bIsFColor = StructName == NAME_Color;
	bool bIsFLinearColor = StructName == NAME_LinearColor;
	bool bIsSlateColor = StructName == FName( "SlateColor" );

	FLinearColor ColorValue;

	if (bIsFColor)
	{
		ColorValue = PropertyChangedParams.GetPropertyValue<FColor>().ReinterpretAsLinear();
	}
	else
	{
		ColorValue = PropertyChangedParams.GetPropertyValue<FLinearColor>();
	}

	if( StructProp->HasMetaData("HideAlphaChannel") )
	{
		ColorValue.A = 1;
	}

	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;

	TArray<FColorKey>& RedKeys = ChannelName == NAME_None || ChannelName == RedName ? NewGeneratedKeys : DefaultGeneratedKeys;
	RedKeys.Add( FColorKey( EKeyColorChannel::Red, ColorValue.R, bIsSlateColor ) );

	TArray<FColorKey>& GreenKeys = ChannelName == NAME_None || ChannelName == GreenName ? NewGeneratedKeys : DefaultGeneratedKeys;
	GreenKeys.Add( FColorKey( EKeyColorChannel::Green, ColorValue.G, bIsSlateColor ) );

	TArray<FColorKey>& BlueKeys =  ChannelName == NAME_None || ChannelName == BlueName ? NewGeneratedKeys : DefaultGeneratedKeys;
	BlueKeys.Add( FColorKey( EKeyColorChannel::Blue, ColorValue.B, bIsSlateColor ) );

	TArray<FColorKey>& AlphaKeys = ChannelName == NAME_None || ChannelName == AlphaName ? NewGeneratedKeys : DefaultGeneratedKeys;
	AlphaKeys.Add( FColorKey( EKeyColorChannel::Alpha, ColorValue.A, bIsSlateColor ) );
}
