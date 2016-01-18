// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UMovieSceneSection;
class IKeyArea;

/**
 * Represents a selected key in the sequencer
 */
struct FSequencerSelectedKey
{	
	/** Section that the key belongs to */
	UMovieSceneSection* Section;
	/** Key area providing the key */
	TSharedPtr<IKeyArea> KeyArea;
	/** Index of the key in the key area */
	TOptional<FKeyHandle> KeyHandle;

	FSequencerSelectedKey( UMovieSceneSection& InSection, TSharedPtr<IKeyArea> InKeyArea, FKeyHandle InKeyHandle )
		: Section( &InSection )
		, KeyArea( InKeyArea )
		, KeyHandle(InKeyHandle)
	{}

	FSequencerSelectedKey()
		: Section( NULL )
		, KeyArea( NULL )
		, KeyHandle()
	{}

	/** @return Whether or not this is a valid selected key */
	bool IsValid() const { return Section != NULL && KeyArea.IsValid() && KeyHandle.IsSet(); }

	friend uint32 GetTypeHash( const FSequencerSelectedKey& SelectedKey )
	{
		return GetTypeHash( SelectedKey.Section ) ^ GetTypeHash( SelectedKey.KeyArea ) ^ 
			(SelectedKey.KeyHandle.IsSet() ? GetTypeHash(SelectedKey.KeyHandle.GetValue()) : 0);
	} 

	bool operator==( const FSequencerSelectedKey& OtherKey ) const
	{
		return Section == OtherKey.Section && KeyArea == OtherKey.KeyArea &&
			KeyHandle.IsSet() && OtherKey.KeyHandle.IsSet() &&
			KeyHandle.GetValue() == OtherKey.KeyHandle.GetValue();
	}

};

