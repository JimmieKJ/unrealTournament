// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneVectorTrack.h"
#include "VectorPropertyTrackEditor.h"
#include "VectorPropertySection.h"

FName FVectorPropertyTrackEditor::XName( "X" );
FName FVectorPropertyTrackEditor::YName( "Y" );
FName FVectorPropertyTrackEditor::ZName( "Z" );
FName FVectorPropertyTrackEditor::WName( "W" );


TSharedRef<ISequencerTrackEditor> FVectorPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FVectorPropertyTrackEditor( InSequencer ) );
}


TSharedRef<FPropertySection> FVectorPropertyTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	return MakeShareable(new FVectorPropertySection(SectionObject, Track.GetDisplayName()));
}


void FVectorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FVectorKey>& GeneratedKeys )
{
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.Last() );

	FName StructName = StructProp->Struct->GetFName();

	bool bIsVector2D = StructName == NAME_Vector2D;
	bool bIsVector = StructName == NAME_Vector;
	bool bIsVector4 = StructName == NAME_Vector4;

	FVector4 VectorValues;
	int32 Channels;

	if ( bIsVector2D )
	{
		FVector2D Vector2DValue = PropertyChangedParams.GetPropertyValue<FVector2D>();
		VectorValues.X = Vector2DValue.X;
		VectorValues.Y = Vector2DValue.Y;
		Channels = 2;
	}
	else if ( bIsVector )
	{
		FVector Vector3DValue = PropertyChangedParams.GetPropertyValue<FVector>();
		VectorValues.X = Vector3DValue.X;
		VectorValues.Y = Vector3DValue.Y;
		VectorValues.Z = Vector3DValue.Z;
		Channels = 3;
	}
	else // if ( bIsVector4 )
	{
		VectorValues = PropertyChangedParams.GetPropertyValue<FVector4>();
		Channels = 4;
	}

	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;
		
	GeneratedKeys.Add( FVectorKey( EKeyVectorChannel::X, VectorValues.X, ChannelName == NAME_None || ChannelName == XName ? EKeyVectorValueType::Key : EKeyVectorValueType::Default ) );
	GeneratedKeys.Add( FVectorKey( EKeyVectorChannel::Y, VectorValues.Y, ChannelName == NAME_None || ChannelName == YName ? EKeyVectorValueType::Key : EKeyVectorValueType::Default ) );
	if (Channels >= 3 )
	{
		GeneratedKeys.Add( FVectorKey( EKeyVectorChannel::Z, VectorValues.Z, ChannelName == NAME_None || ChannelName == ZName ? EKeyVectorValueType::Key : EKeyVectorValueType::Default ) );
	}
	if (Channels >= 4 )
	{
		GeneratedKeys.Add( FVectorKey( EKeyVectorChannel::W, VectorValues.W, ChannelName == NAME_None || ChannelName == WName ? EKeyVectorValueType::Key : EKeyVectorValueType::Default ) );
	}
}

void FVectorPropertyTrackEditor::InitializeNewTrack( UMovieSceneVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams )
{
	FPropertyTrackEditor<UMovieSceneVectorTrack, UMovieSceneVectorSection, FVectorKey>::InitializeNewTrack( NewTrack, PropertyChangedParams );
	const UStructProperty* StructProp = Cast<const UStructProperty>( PropertyChangedParams.PropertyPath.Last() );
	FName StructName = StructProp->Struct->GetFName();

	if ( StructName == NAME_Vector2D )
	{
		NewTrack->SetNumChannelsUsed( 2 );
	}
	if ( StructName == NAME_Vector )
	{
		NewTrack->SetNumChannelsUsed( 3 );
	}
	if ( StructName == NAME_Vector4 )
	{
		NewTrack->SetNumChannelsUsed( 4 );
	}
}

bool FVectorPropertyTrackEditor::ShouldAddKey(UMovieSceneVectorTrack* InTrack, FVectorKey InKey, FKeyParams InKeyParams) const
{
	return FPropertyTrackEditor::ShouldAddKey(InTrack, InKey, InKeyParams) && InKey.ValueType == EKeyVectorValueType::Key;
}
