// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneVectorSection.generated.h"


enum class EKeyVectorChannel
{
	X,
	Y,
	Z,
	W
};


struct MOVIESCENETRACKS_API FVectorKey
{
	FVectorKey( EKeyVectorChannel InChannel, float InValue )
	{
		Channel = InChannel;
		Value = InValue;
	}
	EKeyVectorChannel Channel;
	float Value;
};


/**
 * A vector section
 */
UCLASS(MinimalAPI )
class UMovieSceneVectorSection
	: public UMovieSceneSection
	, public IKeyframeSection<FVectorKey>
{
	GENERATED_UCLASS_BODY()

public:
	
	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	FVector4 Eval( float Position, const FVector4& DefaultVector ) const;

	/** MovieSceneSection interface */
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;


	// IKeyframeSection interface.

	virtual void AddKey( float Time, const FVectorKey& Key, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual bool NewKeyIsNewData( float Time, const FVectorKey& Key ) const override;
	virtual bool HasKeys( const FVectorKey& Key ) const override;
	virtual void SetDefault( const FVectorKey& Key ) override;

	/** Gets one of four curves in this section */
	FRichCurve& GetCurve(const int32& Index) { return Curves[Index]; }

	/** Sets how many channels are to be used */
	void SetChannelsUsed(int32 InChannelsUsed) 
	{
		checkf(InChannelsUsed >= 2 && InChannelsUsed <= 4, TEXT("Only 2-4 channels are supported.") );
		ChannelsUsed = InChannelsUsed;
	}
	
	/** Gets the number of channels in use */
	int32 GetChannelsUsed() const {return ChannelsUsed;}

private:

	/** Vector t */
	UPROPERTY()
	FRichCurve Curves[4];

	/** How many curves are actually used */
	UPROPERTY()
	int32 ChannelsUsed;
};
